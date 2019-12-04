import eyeblink
t = eyeblink.eyeblink() # create an eyeblink object

t.startSession() # start a new session
t.stopSession() # stop a session

t.GetArduinoState() # get the current state with all trial parameters (see Arduino g$
t.settrial('trialDur',3000) # set the value of 'trialDur' trial parameter to 5000 ms
t.settrial('numTrial',1)
t.settrial('useMotor','motorOn')

t.startSession() # start a new trial

