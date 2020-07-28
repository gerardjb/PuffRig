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
t.settrial('numTrial',3)
time.sleep(0.01)
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
        time.sleep(3)
        t.startSession()
    #poll for when the current trial has finished
    while not t.trial['justFinished']:
        time.sleep(0.03)

