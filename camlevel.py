import time
import picamera

with picamera.PiCamera(resolution='160x120',framerate=140) as camera:
    #camera.resolution = (160,120)
    #camera.framerate = 150
    # Wait for the automatic gain control to settle
    camera.start_preview()
    time.sleep(2)
    # Now fix the values
    camera.shutter_speed = camera.exposure_speed
    print("shutter speed" + str(camera.shutter_speed))
    camera.exposure_mode = 'off'
    g = camera.awb_gains
    print("awb gains" + str(g))
    camera.awb_mode = 'off'
    camera.awb_gains = g
    # Finally, take several photos with the fixed settings
    #camera.capture_sequence(['image%02d.jpg' % i for i in range(10)])
    time.sleep(1)
    for filename in camera.capture_continuous('img{timestamp:%Y-%m-%d-%H-%M}.jpg'):
        print('Captured %s' % filename)
        time.sleep(1)
