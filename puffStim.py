'''
Joey Broussard
PNI
20200723
puffStim
Modification of eyeblink code for puff stim production
this is NOT implemented to be a slave
if on Raspberry, using
    - serial to trigger a session/trial
    - serial for event logging
    - serial to stop a trial
    - I2C in arduino code to control treadmill state
    
todo:
    
        
'''

import serial
import time
import os.path
from threading import Thread

import eventlet
eventlet.monkey_patch()
     
#Initialize picamera as a subprocess with pkill
#Serial anf initialization of object settings
serialStr = '/dev/ttyACM0' #uno
altSerialStr = '/dev/ttyACM1' #alternative uno port

options = {}
options['serial'] = {}
options['serial']['port'] = serialStr
options['serial']['baud'] = 115200 #57600
options['picamera'] = 0

trial = {}
trial['filePath'] = ''
trial['fileName'] = ''
trial['sessionNumber'] = 0
trial['sessionDur'] = 0 # (trialDur * numTrial)

trial['trialNumber'] = 0
trial['trialDur'] = 5000
trial['numTrial'] = 3

trial['interTrialInterval'] = 5000 #ms, ITI
trial['prePuffDur'] = 250 #ms, imaged time prior to stim presentation

trial['justFinished'] = False
trial['puffNum'] = 100 #number of puffs to deliver
trial['puffFreq'] =  0.5 #frequency of puff stimulation

#end options
#
            
class eyeblink():
    def __init__(self):
        self.animalID = 'default'
        self.trial = trial
                
        self.socketio = None
        
        try:
            self.ser = serial.Serial(options['serial']['port'], options['serial']['baud'], timeout=0.25)
        except:
            options['serial']['port'] = altSerialStr
            try:
                self.ser = serial.Serial(options['serial']['port'], options['serial']['baud'], timeout = 0.25)

            except:
                self.ser = None
                print "======================================================"
                print "ERROR: treadmill did not find serial port '", options['serial']['port'], "'"
                print "======================================================"

        if options['picamera']:
            print 'treadmill is using raspberry pi camera'
                
        #serial is blocking. we need our trial to run in a separate thread so we do not block user interface
        self.trialRunning = 0
        thread = Thread(target=self.background_thread, args=())
        thread.daemon  = True; #as a daemon the thread will stop when *this stops
        thread.start()
            
        #save all serial data to file, set in setsavepath
        self.savepath = '/media/usb/'
        self.filePtr = None
        
        self.arduinoStateList = None #grab from arduino at start of trial, write into each epoch file
        
        print 'eyeblink.trial:', self.trial
        
    def background_thread(self):
        '''Background thread to continuously read serial. Used during a trial.'''
        while True:
            if self.trialRunning:
                str = self.ser.readline().rstrip()
                if len(str) > 0:
                    print str
                    self.NewSerialData(str)
            time.sleep(0.01)


    def bAttachSocket(self, socketio):
        print 'eyeblink.bAttachSocket() attaching socketio:', socketio
        self.socketio = socketio

    def NewSerialData(self, str):
        '''
        we have received new serial data. pass it back to socketio
        special case is when we receive stopTrial
        '''
        #we want 'millis,event,val', if serial data does not match this then do nothing
        try:
            if len(str)>0:
                    #save to file
                    if self.filePtr:
                        self.filePtr.write(str + '\n')
                    
                    #print "\t=== treadmill.NewSerialData sending serial data to socketio: '" + str + "'"
                    if self.socketio:
                        self.socketio.emit('serialdata', {'data': str})
                    
                    #stop trial
                    parts = str.split(',')
                    if len(parts) > 1:
                        if parts[1] == 'stopTrial':
			    print 'Set justFinished to True'
                            self.trial['justFinished'] = True

                            
        except:
            print "=============="
            print "ERROR: eyeblink.NewSerialData()"
            print "=============="

    def startSession(self):
        if self.trialRunning:
            print 'warning: session is already running'
            return 0
            
        self.trial['sessionNumber'] += 1
        self.trial['trialNumber'] = 0
        
        self.newtrialfile(0)
        
        if self.socketio:
            self.socketio.emit('serialdata', {'data': "=== Session " + str(self.trial['sessionNumber']) + " ==="})
        
	time.sleep(0.01)
        self.ser.write('startSession\n')
        self.trialRunning = 1

        print 'eyeblink.startSession()'
        
        return 1
        
    def startTrial(self):
        if not self.trialRunning:
            print 'warning: startTrial() trial is not running'
            return 0
            
        self.trial['trialNumber'] += 1
                
        self.newtrialfile(self.trial['trialNumber'])

        if self.socketio:
            self.socketio.emit('serialdata', {'data': "=== Trial " + str(self.trial['trialNumber']) + " ==="})
        
        return 1
        
    def stopSession(self):
        if self.filePtr:
            self.filePtr.close()
            self.filePtr = None
            
        self.trialRunning = 0

        self.ser.write('stopSession\n')
        if self.socketio:
            self.socketio.emit('serialdata', {'data': "=== Stop Session " + str(self.trial['sessionNumber']) + " ==="})

        print 'eyeblink.stopSession()'
        
    def newtrialfile(self, trialNumber):
        # open a file for this trial
        dateStr = time.strftime("%Y%m%d")
        timeStr = time.strftime("%H%M%S")
        datetimeStr = dateStr + '_' + timeStr

        sessionStr = ''
        sessionFolder = ''
        if self.animalID and not (self.animalID == 'default'):
            sessionStr = self.animalID + '_'
            sessionFolder = dateStr + '_' + self.animalID
        
        thisSavePath = self.savepath + dateStr + '/'
        if not os.path.exists(thisSavePath):
            os.makedirs(thisSavePath)
        thisSavePath += sessionFolder + '/'
        if not os.path.exists(thisSavePath):
            os.makedirs(thisSavePath)
        
        sessionFileName = sessionStr + datetimeStr + '_s' + str(self.trial['sessionNumber']) + '_t' + str(self.trial['trialNumber']) + '.txt'
        sessionFilePath = thisSavePath + sessionFileName
        
        self.trial['filePath'] = sessionFilePath
        self.trial['fileName'] = sessionFileName
        
        #
        #header line 1 is all arduino parameters
        if trialNumber==0:
            self.arduinoStateList = self.GetArduinoState()      

        self.filePtr = open(sessionFilePath, 'w')

        self.filePtr.write('session='+str(self.trial['sessionNumber'])+';')
        self.filePtr.write('trial='+str(self.trial['trialNumber'])+';')
        self.filePtr.write('date='+dateStr+';')
        self.filePtr.write('time='+timeStr+';')
        
        for state in self.arduinoStateList:
            self.filePtr.write(state + ';')
            
        self.filePtr.write('\n')
        
        #
        #header line 2 is column names
        self.NewSerialData('millis,event,value')

        #
        #each call to self.NewSerialData() will write serial data to this file
        
    def settrial(self, key, val):
        '''
        set value for this trial
        send serial to set value on arduino
        '''
        '''Turned off this block so that trial params can be updated online
            if self.trialRunning:
            print 'warning: trial is already running'
            return 0
        '''
        val = str(val)
        print "=== eyeblink.settrial() key:'" + key + "' val:'" + val + "'"
        if key in self.trial:
            self.trial[key] = val
            serialCommand = 'settrial,' + key + ',' + val 
            serialCommand = str(serialCommand)
            print "\teyeblink.settrial() writing to serial '" + serialCommand + "'"
            self.ser.write(serialCommand + '\n')
        else:
            print '\tERROR: eyeblink:settrial() did not find', key, 'in trial dict'

    def updatetrial(self):
        numTrial = long(self.trial['numTrial'])
        trialDur = long(self.trial['trialDur'])
        totalDur = numTrial * trialDur
        print 'updatesession() set sessionDur=', totalDur
        self.trial['sessionDur'] = str(totalDur)
        
    def GetArduinoState(self):
        if self.trialRunning:
            print 'warning: trial is already running'
            return 0

        if self.socketio:
            self.socketio.emit('serialdata', {'data': "=== Arduino State ==="})
        self.ser.write('getState\n')
        #time.sleep(.02)
        stateList = self.emptySerial()
        if self.socketio:
            self.socketio.emit('serialdata', {'data': "=== Done ==="})
        return stateList
        
    def emptySerial(self):
        if self.trialRunning:
            print 'warning: trial is already running'
            return 0

        theRet = []
        line = self.ser.readline()
        i = 0
        while line:
            line = line.rstrip()
            theRet.append(line)
            self.NewSerialData(line)
            line = self.ser.readline()
            i += 1
        return theRet
        
    def setserialport(self, newPort):
        if self.trialRunning:
            print 'warning: trial is already running'
            return 0

        if os.path.exists(newPort) :
            print 'setserialport() port', newPort, 'exists'
            options['serial']['port'] = newPort
            return 1
        else:
            print 'setserialport() port', newPort, 'does not exist'
            return 0
            
    def checkserialport(self):
        if self.trialRunning:
            print 'warning: trial is already running'
            return 0

        port = options['serial']['port']
        print 'checking', port
        if os.path.exists(port) :
            print 'exists'
            return 1, port
        else:
            print 'does not exist'
            return 0, port
            
    def checkarduinoversion(self):
        if self.trialRunning:
            print 'warning: trial is already running'
            return 0

        self.ser.write('version\n')
        self.emptySerial()
        
    def setsavepath(self, str):
        self.savepath = str
        
    def __del__(self):
        print('Eyeblink object terminated by destructor method')
