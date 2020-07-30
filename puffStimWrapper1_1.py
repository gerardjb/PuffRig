# -*- coding: utf-8 -*-
"""
Created on Thu Jul 23 15:04:35 2020
​
@author: oostland
"""
import pandas as pd
import os
import time
import puffStim
import signal

#Initialize camera
os.system("python3 puffCamera.py &")

#%%
# General parameters
#t = eyeblink.eyeblink()  # I think this can be deleted?
t = puffStim.eyeblink() # create an eyeblink object
t.settrial('numTrial',246)
time.sleep(0.01)

# make a destructor method for this process
def sig_handler(signal,frame):
    print("User ended session")
    print("Process-kill applied to the eyeblinkCamera subprocess")
    os.system("pkill -9 -f puffCamera.py")
    t.stopSession()
    t.__del__
signal.signal(signal.SIGINT, sig_handler)

#%%
# Trial-specific paramaters
# First, open csv file with trial-specific paramaters
#filepath = 'S:\\oostland\\Protocols\\Eyeblink_rig' # Possibly stil change
file = 'puffTime.csv'
df = pd.read_csv(file)


for ind, trialid in enumerate(df.trialid):
    #set the trial parameters prior to initializing each trial
    t.trial['justFinished'] = False
    time.sleep(0.01)
    t.settrial('interTrialInterval', df.iti[trialid]) #ms, lowest ITI from random draw
    time.sleep(0.01)
    t.settrial('puffNum', df.puffNum[trialid]) # number of puffs within this trial
    time.sleep(0.01)
    t.settrial('puffFreq', df.puffFreq[trialid]) # number of puffs within this trial

    if trialid==0:
        print('Got startSession loop')
        raw_input('Hit return when camera is ready')
        t.startSession()
    #poll for when the current trial has finished
    while not t.trial['justFinished']:
        time.sleep(0.03)

t.stopSession()
os.system("pkill -9 -f eyeblinkCamera.py")
print("Process kill applied to eyeblinkCamera subprocess")
