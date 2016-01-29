package android.arduino.bluetooth;

import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.text.method.ScrollingMovementMethod;
import android.util.Log;
import android.view.View;
import android.widget.TextView;
import android.widget.EditText;  
import android.widget.Button;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Set;
import java.util.UUID;
/*
ran notes: 28/1/16 :
-add floating notification about connection, and the name of the BT trying to open.
-add failure handlers to notify the user ,and prevent crushing the proggy.
- -arduino- : fix the controls to allow full and correct operation, and quick reseting option
-add checks to handle machine sleep. otherwise it usually get stuck.
-paste this code in new project with relevant GUI for using the sensors..
-close BT connection on application exit
-all 'myStream_Recived.append' readings can wrap by function the prefix text with ' --- ' and adds '\n' on finish
 */
public class BluetoothTest extends Activity
{
    TextView            myLabel,
                        myStream_Recived;
    EditText            myTextbox;
    BluetoothAdapter    mBluetoothAdapter;
    BluetoothSocket     mmSocket;
    BluetoothDevice     mmDevice;
    OutputStream        mmOutputStream;
    InputStream         mmInputStream;
    Thread              workerThread;
    byte[]              readBuffer;
    int                 readBufferPosition;
    int                 counter;
    volatile boolean    stopWorker;
    
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        
        Button openButton  = (Button)findViewById(R.id.open);
        Button sendButton  = (Button)findViewById(R.id.send);
        Button closeButton = (Button)findViewById(R.id.close);
        myLabel          = (TextView)findViewById(R.id.label);
        myTextbox        = (EditText)findViewById(R.id.entry);
        myStream_Recived = (TextView)findViewById(R.id.streamInput);
        myStream_Recived.setMovementMethod(new ScrollingMovementMethod()); // for adding slidebar

        myStream_Recived.setText("application started.\n ");
        myStream_Recived.append("notice it is better to take care these in advance , before pressing 'OPEN' :.\n ");
        myStream_Recived.append("1.open the BlueTooth unit option.\n ");
        myStream_Recived.append("2.it is searching for 'ranBT' communication unit - otherwise you must change the code.\n ");

        //Open Button
        openButton.setOnClickListener(new View.OnClickListener()
        {
            public void onClick(View v)
            {
                try 
                {
                    findBT();
                    openBT();
                }
                catch (IOException ex) { }
            }
        });
        
        //Send Button
        sendButton.setOnClickListener(new View.OnClickListener()
        {
            public void onClick(View v)
            {
                try 
                {
                    sendData();     // TODO: need to call by user GUI of gesture sensors
                }
                catch (IOException ex) { }
            }
        });
        
        //Close button
        closeButton.setOnClickListener(new View.OnClickListener()
        {
            public void onClick(View v)
            {
                try 
                {
                    closeBT();
                }
                catch (IOException ex) { }
            }
        });
    }
    
    void findBT()
    {
    	Log.i("findBT","in findBT");
    	mBluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
    	Log.i("findBT","bt adapter set");

        if(mBluetoothAdapter == null)
        {
        	Log.i("findBT","null");
            myStream_Recived.append("No bluetooth adapter available\n");
        }
        
        if(!mBluetoothAdapter.isEnabled())
        {
            Intent enableBluetooth = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
            startActivityForResult(enableBluetooth, 0);
            // TODO : add check for failure??
        }
        // TODO : add check for failure of the aboves ??
        Set<BluetoothDevice> pairedDevices = mBluetoothAdapter.getBondedDevices();
    	Log.i("findBT", "paired dev: " + pairedDevices.toString());
        myStream_Recived.append(" -- looking for connection...didn't found yet  -- \n");
        if(pairedDevices.size() > 0)
        {
            for(BluetoothDevice device : pairedDevices)
            {
                //if(device.getName().equals("SeeedBTSlave"))
                if(device.getName().equals("ranBT"))
                    {
                	Log.i("findBT","found seeed");

                    mmDevice = device;
                    myStream_Recived.append("found device ranBT \n");
                    break;
                }
            }
        }
    	Log.i("findBT", "could not find seeed");  //? true?

    }
    
    void openBT() throws IOException
    {
        UUID uuid = UUID.fromString("00001101-0000-1000-8000-00805f9b34fb"); //Standard SerialPortService ID
        mmSocket = mmDevice.createRfcommSocketToServiceRecord(uuid);   
        Log.i("openBT", "after createRf");

        mmSocket.connect();
        Log.i("openBT", "after connect");
        mmOutputStream = mmSocket.getOutputStream();
        mmInputStream  = mmSocket.getInputStream();
        
        beginListenForData();

        myStream_Recived.append("Bluetooth Opened and listening .. \n");
    }
    
    void beginListenForData()
    {
        final Handler handler = new Handler(); 
        final byte delimiter = 10; //This is the ASCII code for a newline character
        
        stopWorker = false;
        readBufferPosition = 0;
        readBuffer = new byte[1024];
        workerThread = new Thread(new Runnable()
        {
            public void run()
            {                
               while(!Thread.currentThread().isInterrupted() && !stopWorker)
               {
                    try 
                    {
                        int bytesAvailable = mmInputStream.available();                        
                        if(bytesAvailable > 0)
                        {
                            byte[] packetBytes = new byte[bytesAvailable];
                            mmInputStream.read(packetBytes);
                            for(int i=0;i<bytesAvailable;i++)
                            {
                                byte b = packetBytes[i];
                                if(b == delimiter)         // delimiter is char / ascii of 10
                                {
                                    byte[] encodedBytes = new byte[readBufferPosition];
                                    System.arraycopy(readBuffer, 0, encodedBytes, 0, encodedBytes.length);
                                    final String data = new String(encodedBytes, "US-ASCII");
                                    readBufferPosition = 0;
                                    
                                    handler.post(new Runnable()
                                    {
                                        public void run()
                                        {
                                            myStream_Recived.append(data + "\n");
                                            // using : http://stackoverflow.com/questions/3506696/auto-scrolling-textview-in-android-to-bring-text-into-view
                                            final int scrollAmount =
                                                    myStream_Recived.getLayout().getLineTop(myStream_Recived.getLineCount()) - myStream_Recived.getHeight();
                                            // if there is no need to scroll, scrollAmount will be <=0
                                            if (scrollAmount > 0)
                                                myStream_Recived.scrollTo(0, scrollAmount);
                                            else
                                                myStream_Recived.scrollTo(0, 0);
                                        }
                                    });
                                }
                                else
                                {
                                    readBuffer[readBufferPosition++] = b;
                                }
                            }
                        }
                    } 
                    catch (IOException ex) 
                    {
                        stopWorker = true;
                    }
               }
            }
        });

        workerThread.start();
    }
    
    void sendData() throws IOException
    {
        String msg = myTextbox.getText().toString();
        msg += "\n";

        mmOutputStream.write(msg.getBytes());

        myStream_Recived.append("Data : " + msg + "Sent \n");
    }
    
    void closeBT() throws IOException
    {
        stopWorker = true;

        mmOutputStream.close();
        mmInputStream.close();
        mmSocket.close();

        myStream_Recived.append("Bluetooth Closed now \n");
    }
}
