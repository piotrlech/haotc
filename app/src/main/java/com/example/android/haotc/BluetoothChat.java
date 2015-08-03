/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.example.android.haotc;

import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.SystemClock;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.Window;
import android.view.inputmethod.EditorInfo;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ToggleButton;

import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.zip.CRC32;

/**
 * This is the main Activity that displays the current chat session.
 */
public class BluetoothChat extends Activity {
    // Debugging
    private static final String TAG = "BluetoothChat";
    private static final boolean D = false;
    private static final boolean BtAvailable = true;

    // Message types sent from the BluetoothChatService Handler
    public static final int MESSAGE_STATE_CHANGE = 1;
    public static final int MESSAGE_READ = 2;
    public static final int MESSAGE_WRITE = 3;
    public static final int MESSAGE_DEVICE_NAME = 4;
    public static final int MESSAGE_TOAST = 5;

    // Key names received from the BluetoothChatService Handler
    public static final String DEVICE_NAME = "device_name";
    public static final String TOAST = "toast";

    // Intent request codes
    private static final int REQUEST_CONNECT_DEVICE = 1;
    private static final int REQUEST_ENABLE_BT = 2;
    private static final int REQUEST_CONNECT_INIT = 3;

    // Layout Views
    private TextView mTitle;
    private ListView mConversationView;
    //private EditText mOutEditText;
    private Button mReadButton;
    private Button mSendButton;
    private ToggleButton mButton1;
    private String readMessageBuffer = "";

    // Name of the connected device
    private String mConnectedDeviceName = null;
    // Array adapter for the conversation thread
    private ArrayAdapter<String> mConversationArrayAdapter;
    // String buffer for outgoing messages
    private StringBuffer mOutStringBuffer;
    // Local Bluetooth adapter
    private BluetoothAdapter mBluetoothAdapter = null;
    // Member object for the chat services
    private BluetoothChatService mChatService = null;

    private final ScheduledExecutorService scheduler = Executors.newScheduledThreadPool(1);

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if(D) Log.e(TAG, "+++ ON CREATE +++");

        // Set up the window layout
        requestWindowFeature(Window.FEATURE_CUSTOM_TITLE);
        setContentView(R.layout.main);
        getWindow().setFeatureInt(Window.FEATURE_CUSTOM_TITLE, R.layout.custom_title);

        // Set up the custom title
        mTitle = (TextView) findViewById(R.id.title_left_text);
        mTitle.setText(R.string.app_name);
        mTitle = (TextView) findViewById(R.id.title_right_text);

        // Get local Bluetooth adapter
        mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();

        // If the adapter is null, then Bluetooth is not supported
        if (BtAvailable && mBluetoothAdapter == null) {
            Toast.makeText(this, "Bluetooth is not available", Toast.LENGTH_LONG).show();
            finish();
            return;
        }
        //
    }

    @Override
    public void onStart() {
        super.onStart();
        if(D) Log.e(TAG, "++ ON START ++");

        //super.onCreate(savedInstanceState);
        //scheduleTaskExecutor= Executors.newScheduledThreadPool(5);

        // This schedule a task to run every 10 minutes:
        scheduler.scheduleAtFixedRate(new Runnable() {
            public void run() {
                if (D) Log.e(TAG, "*** scheduled job ***");
                SimpleDateFormat formatter = new SimpleDateFormat("HH,mm,ss");
                //DateFormat dateFormat = new SimpleDateFormat("yyyy/MM/dd HH:mm:ss");
                Date date = new Date();
                //System.out.println(formatter.format(date));
                String message = "*11," + formatter.format(date) + "#";
                if (D) Log.e(TAG, message);
                sendMessage(message);
                //sendTriggers(1, R.id.button1on, R.id.button1off);

                //mReadButton = (Button) findViewById(R.id.button_read);
                //mReadButton.performClick();
                // If you need update UI, simply do this:
                //runOnUiThread(new Runnable() {
                //    public void run() {
                //        // update your UI component here.
                //        myTextView.setText("refreshed");
                //    }
                //});
            }
        }, 2, 60, TimeUnit.SECONDS);

        if(D) Log.e(TAG, "+++ ON START +++");
        // If BT is not on, request that it be enabled.
        // setupChat() will then be called during onActivityResult
        if(BtAvailable) {
            if (!mBluetoothAdapter.isEnabled()) {
                Intent enableIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
                startActivityForResult(enableIntent, REQUEST_ENABLE_BT);
                // Otherwise, setup the chat session
            } else {
                // Get the address
                SharedPreferences sharedPref = getPreferences(Context.MODE_PRIVATE);
                String address = sharedPref.getString("address", "");
                // Get the BLuetoothDevice object
                if (D) Log.e(TAG, "stored address: " + address);
                if (address.length() > 15) {
                    BluetoothDevice device = mBluetoothAdapter.getRemoteDevice(address);
                    // Attempt to connect to the device
                    if (mChatService == null) setupChat();
                    if (mChatService != null) mChatService.connect(device);
                }
            }
        }
    }

    @Override
    public synchronized void onResume() {
        super.onResume();
        if(D) Log.e(TAG, "+ ON RESUME +");

        // Performing this check in onResume() covers the case in which BT was
        // not enabled during onStart(), so we were paused to enable it...
        // onResume() will be called when ACTION_REQUEST_ENABLE activity returns.
        if (mChatService != null) {
            // Only if the state is STATE_NONE, do we know that we haven't started already
            if (mChatService.getState() == BluetoothChatService.STATE_NONE) {
              // Start the Bluetooth chat services
              mChatService.start();
            }
            /*/ piotr - scheduler
            public void beepForAnHour() {
            final Runnable beeper = new Runnable() {
                public void run() { System.out.println("beep");
                };
                final ScheduledFuture beeperHandle =
                        scheduler.scheduleAtFixedRate(beeper, 10, 10, SECONDS);
                scheduler.schedule(new Runnable() {
                    public void run() { beeperHandle.cancel(true); }
                }, 60 * 60, SECONDS);
            }
            // another attempt
            long ms = SystemClock.uptimeMillis();
            while (mChatService.getState() != BluetoothChatService.STATE_CONNECTED && SystemClock.uptimeMillis()-ms < 1000)
                ;
            if (mChatService.getState() == BluetoothChatService.STATE_CONNECTED) {
                mReadButton = (Button) findViewById(R.id.button_read);
                mReadButton.performClick();
            }
            // ~piotr */
        }
    }


    private void setupChat() {
        Log.d(TAG, "setupChat()");

        // Initialize the array adapter for the conversation thread
        mConversationArrayAdapter = new ArrayAdapter<String>(this, R.layout.message);
        mConversationView = (ListView) findViewById(R.id.in);
        mConversationView.setAdapter(mConversationArrayAdapter);

        // Initialize the send button with a listener that for click events
        mButton1 = (ToggleButton) findViewById(R.id.toggleButton1);
        mButton1.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
            	boolean isOn = ((ToggleButton) v).isChecked();
            	if(isOn)
            		sendMessage("*10,1,3#");
            	else
            		sendMessage("*10,1,2#");
            }
        });

        // Initialize the send button with a listener that for click events
        mButton1 = (ToggleButton) findViewById(R.id.toggleButton2);
        mButton1.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
            	boolean isOn = ((ToggleButton) v).isChecked();
            	if(isOn)
            		sendMessage("*10,2,3#");
            	else
            		sendMessage("*10,2,2#");
            }
        });

        // Initialize the send button with a listener that for click events
        mButton1 = (ToggleButton) findViewById(R.id.toggleButton3);
        mButton1.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
            	boolean isOn = ((ToggleButton) v).isChecked();
            	if(isOn)
            		sendMessage("*10,3,3#");
            	else
            		sendMessage("*10,3,2#");
            }
        });

        // Initialize the send button with a listener that for click events
        mButton1 = (ToggleButton) findViewById(R.id.toggleButton4);
        mButton1.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
            	boolean isOn = ((ToggleButton) v).isChecked();
            	if(isOn)
            		sendMessage("*10,4,3#");
            	else
            		sendMessage("*10,4,2#");
            }
        });

        // Initialize the send button with a listener that for click events
        mButton1 = (ToggleButton) findViewById(R.id.toggleButton5);
        mButton1.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
            	boolean isOn = ((ToggleButton) v).isChecked();
            	if(isOn)
            		sendMessage("*10,5,3#");
            	else
            		sendMessage("*10,5,2#");
            }
        });

        // Initialize the send button with a listener that for click events
        mButton1 = (ToggleButton) findViewById(R.id.toggleButton6);
        mButton1.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
            	boolean isOn = ((ToggleButton) v).isChecked();
            	if(isOn)
            		sendMessage("*10,6,3#");
            	else
            		sendMessage("*10,6,2#");
            }
        });

        mReadButton = (Button) findViewById(R.id.button_read);
        mReadButton.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                SimpleDateFormat formatter = new SimpleDateFormat("HH,mm,ss");
            	//DateFormat dateFormat = new SimpleDateFormat("yyyy/MM/dd HH:mm:ss");
            	Date date = new Date();
            	//System.out.println(formatter.format(date));
                String message = "*11," + formatter.format(date) + "#";
                Log.d(TAG, message);
                sendMessage(message);
                //sendTriggers(1, R.id.button1on, R.id.button1off);
            }
        });

        // Initialize the compose field with a listener for the return key
        //mOutEditText = (EditText) findViewById(R.id.edit_text_out);
        //mOutEditText.setOnEditorActionListener(mWriteListener);

        // Initialize the send button with a listener that for click events
        mSendButton = (Button) findViewById(R.id.button_send);
        mSendButton.setOnClickListener(new OnClickListener() {
            public void onClick(View v) {
                SimpleDateFormat formatter = new SimpleDateFormat("yy.MM.dd HH:mm:ss");
                Date mDate = new Date();
            	String sFormatted = formatter.format(mDate);
                String message;
                message = "*12,0,0,0#";
                sendMessage(message);
                sendTriggers(1, R.id.button1on, R.id.button1off);
                sendTriggers(2, R.id.button2on, R.id.button2off);
                sendTriggers(3, R.id.button3on, R.id.button3off);
                sendTriggers(4, R.id.button4on, R.id.button4off);
                sendTriggers(5, R.id.button5on, R.id.button5off);
                sendTriggers(6, R.id.button6on, R.id.button6off);
                message = "*12,0,1#";
                sendMessage(message);
                Log.w(TAG, message);
                SystemClock.sleep(200);
                message = "*13," + sFormatted + "#";
                sendMessage(message);
            }
        });

        // Initialize the BluetoothChatService to perform bluetooth connections
        mChatService = new BluetoothChatService(this, mHandler);

        // Initialize the buffer for outgoing messages
        mOutStringBuffer = new StringBuffer("");
    }

    private void sendTriggers(int buttonNr, int buttonOn, int buttonOff) {
        SimpleDateFormat formatter = new SimpleDateFormat("HH:mm");
        TextView tvButton = (TextView) findViewById(buttonOn);
        String sTime = tvButton.getText().toString();
        try {
        	Date mDate = (Date)formatter.parse(sTime);
        	String sFormatted = formatter.format(mDate);
            String message = "*12," + buttonNr + ",3," + sFormatted + "," + buttonNr  + "###";
            Log.i(TAG, message);
            sendMessage(message);
		} catch (ParseException e) {
		}
        tvButton = (TextView) findViewById(buttonOff);
        sTime = tvButton.getText().toString();
        try {
        	Date mDate = (Date)formatter.parse(sTime);
        	String sFormatted = formatter.format(mDate);
            String message = "*12," + buttonNr + ",2," + sFormatted + "," + buttonNr  + "###";
            Log.v(TAG, message);
            sendMessage(message);
		} catch (ParseException e) {
		}
        SystemClock.sleep(500);
    }
        
    @Override
    public synchronized void onPause() {
        super.onPause();
        if(D) Log.e(TAG, "- ON PAUSE -");
    }

    @Override
    public void onStop() {
        super.onStop();
        if(D) Log.e(TAG, "-- ON STOP --");
        scheduler.shutdownNow();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        // Stop the Bluetooth chat services
        if (mChatService != null) mChatService.stop();
        if(D) Log.e(TAG, "--- ON DESTROY ---");
    }

    private void ensureDiscoverable() {
        if(D) Log.d(TAG, "ensure discoverable");
        if (mBluetoothAdapter.getScanMode() !=
            BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE) {
            Intent discoverableIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE);
            discoverableIntent.putExtra(BluetoothAdapter.EXTRA_DISCOVERABLE_DURATION, 300);
            startActivity(discoverableIntent);
        }
    }

    /**
     * Sends a message.
     * @param message  A string of text to send.
     */
    private void sendMessage(String message) {
        // Check that we're actually connected before trying anything
        if (mChatService.getState() != BluetoothChatService.STATE_CONNECTED) {
            Toast.makeText(this, R.string.not_connected, Toast.LENGTH_SHORT).show();
            return;
        }

        // Check that there's actually something to send
        if (message.length() > 0) {
            // crc32
            CRC32 crc = new CRC32();
            crc.update(message.getBytes());
            Log.e(TAG, Long.toHexString(crc.getValue()));

            // Get the message bytes and tell the BluetoothChatService to write
            byte[] send = message.getBytes();
            mChatService.write(send);

            // Reset out string buffer to zero and clear the edit text field
            mOutStringBuffer.setLength(0);
            //mOutEditText.setText(mOutStringBuffer);
        }
    }

    // The action listener for the EditText widget, to listen for the return key
    private TextView.OnEditorActionListener mWriteListener =
        new TextView.OnEditorActionListener() {
        public boolean onEditorAction(TextView view, int actionId, KeyEvent event) {
            // If the action is a key-up event on the return key, send the message
            if (actionId == EditorInfo.IME_NULL && event.getAction() == KeyEvent.ACTION_UP) {
                String message = view.getText().toString();
                sendMessage(message);
            }
            if(D) Log.i(TAG, "END onEditorAction");
            return true;
        }
    };

    // The Handler that gets information back from the BluetoothChatService
    private final Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
            case MESSAGE_STATE_CHANGE:
                if(D) Log.i(TAG, "MESSAGE_STATE_CHANGE: " + msg.arg1);
                switch (msg.arg1) {
                case BluetoothChatService.STATE_CONNECTED:
                    mTitle.setText(R.string.title_connected_to);
                    mTitle.append(mConnectedDeviceName);
                    mConversationArrayAdapter.clear();
                    String message = "Just connected";
                    byte[] send = message.getBytes();
                    mChatService.write(send);
                    break;
                case BluetoothChatService.STATE_CONNECTING:
                    mTitle.setText(R.string.title_connecting);
                    break;
                case BluetoothChatService.STATE_LISTEN:
                case BluetoothChatService.STATE_NONE:
                    mTitle.setText(R.string.title_not_connected);
                    break;
                }
                break;
            case MESSAGE_WRITE:
                byte[] writeBuf = (byte[]) msg.obj;
                // construct a string from the buffer
                String writeMessage = new String(writeBuf);
                //mConversationArrayAdapter.add("Me:  " + writeMessage);
                break;
            case MESSAGE_READ:
                byte[] readBuf = (byte[]) msg.obj;
                // construct a string from the valid bytes in the buffer
                String readMessage = new String(readBuf, 0, msg.arg1);
        		readMessageBuffer = readMessageBuffer + readMessage;
                while(readMessageBuffer.contains("\n")) {
        			//System.out.println("readMessageBuffer 1--" + readMessageBuffer + "--1");
        			int nLineAt = readMessageBuffer.indexOf("\n");
        			//System.out.println("nLineAt = " + nLineAt);
        			String fLine = readMessageBuffer.substring(0, Math.max(0, nLineAt-1));
        			//Log.w(TAG, "SEND IT OUT = " + fLine);
                    if(fLine.length() > 0) {
                        mConversationArrayAdapter.add(fLine);
                    	String[] parts = fLine.split(",|:|#");
                    	if(parts != null && parts.length > 1) {
                    		String outcome = "--" + fLine + "--";
                    		for(int i = 0; i < parts.length; i++)
                    			outcome = outcome + parts[i] + "++";
                    		Log.i(TAG, outcome);
                    		if(parts[0].equals("Schedule")) {
                    			try {
	                    			int nButton = Integer.parseInt(parts[1]);
	                            	TextView tvButton;
	                            	int h1 = Integer.parseInt(parts[2].trim());
	                            	int m1 = Integer.parseInt(parts[3].trim());
	                            	int h2 = Integer.parseInt(parts[4].trim());
	                            	int m2 = Integer.parseInt(parts[5].trim());
	                            	String sTimeOn = "";
	                            	String sTimeOff = "";
	                            	if(h1 >= 0 && h1 <=23 && m1 >=0 && m1 <=59) {
		                            	sTimeOn  = String.format("%02d", h1) + ":" + String.format("%02d", m1);
	                            	}
	                            	if(h2 >= 0 && h2 <=23 && m2 >=0 && m2 <=59) {
		                            	sTimeOff = String.format("%02d", h2) + ":" + String.format("%02d", m2);
	                            	}
	                    			switch (nButton) {
	                    			case 1:
	                                    tvButton = (TextView) findViewById(R.id.button1on);
	                                    tvButton.setText(sTimeOn);
	                                    tvButton = (TextView) findViewById(R.id.button1off);
	                                    tvButton.setText(sTimeOff);
	                                    break;
	                    			case 2:
	                                    tvButton = (TextView) findViewById(R.id.button2on);
	                                    tvButton.setText(sTimeOn);
	                                    tvButton = (TextView) findViewById(R.id.button2off);
	                                    tvButton.setText(sTimeOff);
	                                    break;
	                    			case 3:
	                                    tvButton = (TextView) findViewById(R.id.button3on);
	                                    tvButton.setText(sTimeOn);
	                                    tvButton = (TextView) findViewById(R.id.button3off);
	                                    tvButton.setText(sTimeOff);
	                                    break;
	                    			case 4:
	                                    tvButton = (TextView) findViewById(R.id.button4on);
	                                    tvButton.setText(sTimeOn);
	                                    tvButton = (TextView) findViewById(R.id.button4off);
	                                    tvButton.setText(sTimeOff);
	                                    break;
	                    			case 5:
	                                    tvButton = (TextView) findViewById(R.id.button5on);
	                                    tvButton.setText(sTimeOn);
	                                    tvButton = (TextView) findViewById(R.id.button5off);
	                                    tvButton.setText(sTimeOff);
	                                    break;
	                    			case 6:
	                                    tvButton = (TextView) findViewById(R.id.button6on);
	                                    tvButton.setText(sTimeOn);
	                                    tvButton = (TextView) findViewById(R.id.button6off);
	                                    tvButton.setText(sTimeOff);
	                                    break;
	                    			}
                    			}catch(NumberFormatException ex){}
                    		}

                    		if(parts[0].equals("Status")) {
                    			try {
	                    			ToggleButton tButton;
	                            	for(int i = 0; i < Math.min(6, parts.length+1); i++) {
	                            		int s = Integer.parseInt(parts[i+1].trim());
	                            		switch (i+1) {
	                            		case 1:
	                                        tButton = (ToggleButton) findViewById(R.id.toggleButton1);
	                                        tButton.setChecked(s == 1);
	                            			break;
	                            		case 2:
	                                        tButton = (ToggleButton) findViewById(R.id.toggleButton2);
	                                        tButton.setChecked(s == 1);
	                            			break;
	                            		case 3:
	                                        tButton = (ToggleButton) findViewById(R.id.toggleButton3);
	                                        tButton.setChecked(s == 1);
	                            			break;
	                            		case 4:
	                                        tButton = (ToggleButton) findViewById(R.id.toggleButton4);
	                                        tButton.setChecked(s == 1);
	                            			break;
	                            		case 5:
	                                        tButton = (ToggleButton) findViewById(R.id.toggleButton5);
	                                        tButton.setChecked(s == 1);
	                            			break;
	                            		case 6:
	                                        tButton = (ToggleButton) findViewById(R.id.toggleButton6);
	                                        tButton.setChecked(s == 1);
	                            			break;
	                            		}
	                            	}
                    			}catch(NumberFormatException ex){}
                    		}
                    		
                    	}
                    }
        			String sLine = readMessageBuffer.substring(Math.min(nLineAt+1, readMessageBuffer.length()));
        			readMessageBuffer = sLine;
        		}
                //mConversationArrayAdapter.add(readMessage);
                break;
            case MESSAGE_DEVICE_NAME:
                // save the connected device's name
                mConnectedDeviceName = msg.getData().getString(DEVICE_NAME);
                Toast.makeText(getApplicationContext(), "Connected to "
                               + mConnectedDeviceName, Toast.LENGTH_SHORT).show();
                break;
            case MESSAGE_TOAST:
                Toast.makeText(getApplicationContext(), msg.getData().getString(TOAST),
                               Toast.LENGTH_SHORT).show();
                break;
            }
        }
    };

    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if(D) Log.w(TAG, "onActivityResult " + resultCode);
        switch (requestCode) {
            case REQUEST_CONNECT_INIT:
                if(D) Log.w(TAG, "*** REQUEST_CONNECT_INIT ***");
                // Connect on startup (piotr)
                if (resultCode == Activity.RESULT_OK) {
                    // Get the device MAC address
                    String address = data.getExtras()
                            .getString(DeviceListActivity.EXTRA_DEVICE_ADDRESS);
                    // Store the address
                    SharedPreferences sharedPref = getPreferences(Context.MODE_PRIVATE);
                    SharedPreferences.Editor editor = sharedPref.edit();
                    editor.putString("address", address);
                    editor.commit();
                    // Get the BLuetoothDevice object
                    BluetoothDevice device = mBluetoothAdapter.getRemoteDevice(address);
                    // Attempt to connect to the device
                    mChatService.connect(device);
                }
                break;
            case REQUEST_CONNECT_DEVICE:
                if(D) Log.w(TAG, "*** REQUEST_CONNECT_DEVICE ***");
                // When DeviceListActivity returns with a device to connect
                if (resultCode == Activity.RESULT_OK) {
                    // Get the device MAC address
                    String address = data.getExtras()
                            .getString(DeviceListActivity.EXTRA_DEVICE_ADDRESS);
                    // Store the address
                    SharedPreferences sharedPref = getPreferences(Context.MODE_PRIVATE);
                    SharedPreferences.Editor editor = sharedPref.edit();
                    editor.putString("address", address);
                    editor.commit();
                    // Get the BLuetoothDevice object
                    BluetoothDevice device = mBluetoothAdapter.getRemoteDevice(address);
                    // Attempt to connect to the device
                    mChatService.connect(device);
                }
                break;
        case REQUEST_ENABLE_BT:
            // When the request to enable Bluetooth returns
            if (resultCode == Activity.RESULT_OK) {
                // Bluetooth is now enabled, so set up a chat session
                setupChat();
            } else {
                // User did not enable Bluetooth or an error occured
                Log.d(TAG, "BT not enabled");
                Toast.makeText(this, R.string.bt_not_enabled_leaving, Toast.LENGTH_SHORT).show();
                finish();
            }
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.option_menu, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
        case R.id.scan:
            // Launch the DeviceListActivity to see devices and do scan
            Intent serverIntent = new Intent(this, DeviceListActivity.class);
            startActivityForResult(serverIntent, REQUEST_CONNECT_DEVICE);
            return true;
        case R.id.discoverable:
            // Ensure this device is discoverable by others
            ensureDiscoverable();
            return true;
        }
        return false;
    }

}