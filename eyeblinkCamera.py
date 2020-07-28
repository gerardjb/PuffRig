# Web streaming and save split under RPi.GPIO control
# Todo: instantiate as an object, test that it runs as expected
#   put under RPi.GPIO controls - add interrupts?
#   fold into the eyeblink code
#       share filenames
#       share timestamps
#       dynamically create save folders, file names
#       dynamically mv files using bash during ITIs
#   clean up libraries that aren't being used
#       

import io
import picamera
from picamera import mmal, mmalobj as mo
import logging
import socketserver
from threading import Condition,Thread
from http import server
import datetime as dt
import RPi.GPIO as GPIO
import time
#import cv2
import pickle
import csv
import os

#Breaking into the stream, clocks
global camera
camera = []
global streamOn
streamOn = False
global firstFrame_idx
firstFrame_idx = 0
GPUtimer = mo.MMALCamera()
global outdata
global trialNum
trialNum = 0
global justOff
justOff = False

#Setting up the GPIO interface
GPIO.setwarnings(False)
GPIO.cleanup()
GPIO.setmode(GPIO.BCM)
on_pin = 27
GPIO.setup(on_pin,GPIO.IN,pull_up_down=GPIO.PUD_UP)
led_pin = 4
GPIO.setup(led_pin,GPIO.OUT)
GPIO.output(led_pin, GPIO.LOW)

PAGE="""\
<html>
<head>
<title>Eyeblinlk picamera</title>
</head>
<body>
<center><h1>Eyeblink picamera</h1></center>
<center><img src="stream.mjpg" width="640" height="480"></center>
</body>
</html>
"""

class masterStream:
    def __init__(self):
        thread = Thread(target = self.startCamera(),args=())
        thread.daemon = True
        thread.run()
        
    def get_millis():
        millis = (round(GPUtimer.control.params[mmal.MMAL_PARAMETER_SYSTEM_TIME]/1000))
        return millis
    
    def interrupt_on(on_pin):
        #What we do when low-level interrupt pin goes high
        global streamOn
        global justOff
        print('streamOn = '+ str(streamOn))
        if streamOn and not justOff:
            global camera
            global trial_start
            global d
            global firstFrame_idx
            global outdata
            #Get time and frame at on TTL receipt
            trial_start = masterStream.get_millis()
            firstFrame_idx = camera.frame.index
            #Take down idle banner and initialize data outputs
            camera.annotate_text = ''
            #Can keep metadata as tuples (stamp,frameNum,trial)
            d = []
            #Write bytes directly from stream to this object
            outdata = []
            print('Start TTL received at '+str(camera.frame.timestamp/1000 - trial_start) +'\n relative to first frame grab')


    class StreamingOutput(object):
        def __init__(self):
            self.frame = None
            self.buffer = io.BytesIO()
            self.condition = Condition()
    
        def write(self, buf):
            if buf.startswith(b'\xff\xd8'):
                # New frame, copy the existing buffer's content and notify all
                # clients it's available
                self.buffer.truncate()
                with self.condition:
                    self.frame = self.buffer.getvalue()
                    self.condition.notify_all()
                self.buffer.seek(0)
            return self.buffer.write(buf)
            

    class StreamingHandler(server.BaseHTTPRequestHandler):          
        
        def do_GET(self):
            if self.path == '/':
                self.send_response(301)
                self.send_header('Location', '/index.html')
                self.end_headers()
            elif self.path == '/index.html':
                content = PAGE.encode('utf-8')
                self.send_response(200)
                self.send_header('Content-Type', 'text/html')
                self.send_header('Content-Length', len(content))
                self.end_headers()
                self.wfile.write(content)
            elif self.path == '/stream.mjpg':
                self.send_response(200)
                self.send_header('Age', 0)
                self.send_header('Cache-Control', 'no-cache, private')
                self.send_header('Pragma', 'no-cache')
                self.send_header('Content-Type', 'multipart/x-mixed-replace; boundary=FRAME')
                self.end_headers()
                try:
                    global streamOn
                    streamOn = True
                    while True:
                        global output
                        with output.condition:
                            output.condition.wait()
                            frame = output.frame
                        self.wfile.write(b'--FRAME\r\n')
                        self.send_header('Content-Type', 'image/mjpg')
                        self.send_header('Content-Length', len(frame))
                        self.end_headers()
                        self.wfile.write(frame)
                        self.wfile.write(b'\r\n')
                        global camera
                        if GPIO.input(on_pin) and 'd' in globals():
                            global trial_start
                            global firstFrame_idx
                            global d
                            global outdata
                            global trialNum
                            frame_millis = int(round(camera.frame.timestamp/1000)) - trial_start
                            frame_idx = camera.frame.index - firstFrame_idx
                            #python lib of tuples with (timestamp,frameNum,trial)
                            d.append((frame_millis,frame_idx,trialNum))
                            #Pickling the frame data, save and puch during ITI
                            outdata.append(frame)
                            global justOff
                            justOff = True
                        elif ~GPIO.input(on_pin) and justOff:
                            t = time.time()
                            camera.annotate_text_size = 6
                            camera.annotate_text = 'off'
                            #Also do initialization of data paths
                            savePathInit = '/media/usb/'
                            dateStr = time.strftime("%Y%m%d")
                            savePath = savePathInit + dateStr + '/'
                            if not os.path.exists(savePath):
                                os.mkdir(savePath)
                            trialNum += 1
                            dateStr = time.strftime("%Y%m%d")
                            timeStr = time.strftime("%H%M%S")
                            datetimeStr = dateStr + '_' + timeStr
                            fName = savePath + datetimeStr + 'cam_t' + str(trialNum)
                            #Contains metadata
                            csvfName = fName + '.csv'
                            #Contains stream captures of mjpeg encoded image data
                            bytefName = fName + '.data'
                            #csv save the metadata
                            with open(csvfName,'w',newline = '') as outMetadata:
                                wr = csv.writer(outMetadata)
                                wr.writerows(d)
                                print('Wrote '+csvfName)
                            #pickle the image data
                            fByte = open(bytefName,'wb')
                            pickle.dump(outdata,fByte)
                            print('Wrote '+ bytefName)
                            elapsed = time.time() - t
                            print('Writing took '+str(elapsed))
                            justOff = False
                
                except KeyboardInterrupt:
                    print('Keyboard interrupt ended stream')
                    camera.close()
                    server.server_close()
                    GPIO.cleanup()
                    pass
                
                except Exception as e:
                    camera.close()
                    GPIO.cleanup()
                    logging.warning(
                        'Removed streaming client %s: %s',
                        self.client_address, str(e))
            else:
               self.send_error(404)
               self.end_headers()

    class StreamingServer(socketserver.ThreadingMixIn, server.HTTPServer):
        
        allow_reuse_address = True
        daemon_threads = True
        
    def startCamera(self):
        global camera
        with picamera.PiCamera(resolution='160x128', framerate=150) as camera:
            global output
            output = masterStream.StreamingOutput()
            #Uncomment the next line to change your Pi's Camera rotation (in degrees)
            camera.rotation = 180
            #Doing the static levels on camera
            time.sleep(2) #let the levels settle
            camera.shutter_speed = camera.exposure_speed
            camera.exposure_mode = 'off'
            g = camera.awb_gains
            camera.awb_mode = 'off'
            camera.awb_gains = g
            #Setting up camera clock, outputs, and initial image annotation
            camera.clock_mode = 'raw'
            camera.start_recording(output, format='mjpeg')
            camera.annotate_background = picamera.Color('black')
            camera.annotate_text_size = 6
            camera.annotate_text = dt.datetime.now().strftime('%Y-%m-%d %H:%M:%S')
            
            #Setting up the low level interrupts with optional LED vis
            GPIO.add_event_detect(on_pin,GPIO.RISING,callback=masterStream.interrupt_on)   

            try:
                global address
                address = ('', 8000)
                server = masterStream.StreamingServer(address, masterStream.StreamingHandler)
                server.serve_forever()                
            except KeyboardInterrupt:
                    print('Keyboard interrupt ended stream at address',address)
                    camera.close()
                    server.server_close()
                    GPIO.cleanup()
                    pass            
            finally:
                camera.close()
                server.server_close()
                GPIO.cleanup()
            

if __name__ == "__main__":
    master = masterStream()

