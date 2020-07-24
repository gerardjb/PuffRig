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
#%%
# General parameters
#t = eyeblink.eyeblink()  # I think this can be deleted?
t = puffStim.eyeblink() # create an eyeblink object
t.settrial('filePath', '') # Still change
t.settrial('fileName', '') # Still change
t.settrial('numTrial',3)

#%%
# Trial-specific paramaters
# First, open csv file with trial-specific paramaters
#filepath = 'S:\\oostland\\Protocols\\Eyeblink_rig' # Possibly stil change
file = 'puffTime.csv'
df = pd.read_csv(file)

#Set the parameters for the initial trial and start session
t.settrial('prePuffDur',250)# currently setting 250 millis of trial prior to first puff
t.settrial('puffNum',df.puffNum[0])
t.settrial('puffFreq',df.puffFreq[0])
t.settrial('interTrialInterval',df.iti[0])
t.startSession()

for ind, trialid in enumerate(df.trialid):
    #poll for when the current trial has finished
    while not t.trial['justFinished']:
        time.sleep(0.01)

    #reset the trial parameters at the completion of each trial
    t.trial['justFinished'] = False
    t.settrial('trialDur', df.totalRecDur[trialid]) # set the value of 'trialDur' trial parameter to 5000 ms
    t.settrial('interTrialInterval', df.iti[trialid]) #ms, lowest ITI from random draw
    t.settrial('puffNum', df.puffNum[trialid]) # number of puffs within this trial
    t.settrial('puffFreq', df.puffFreq[trialid]) # number of puffs within this trial
