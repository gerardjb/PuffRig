# -*- coding: utf-8 -*-
"""
Created on Sat Jan 25 12:26:07 2020

@author: wanglab
"""
#%%
import bokeh.plotting as bpl
import cv2
import glob
import logging
import matplotlib.pyplot as plt
import numpy as np
import os

try:
    cv2.setNumThreads(0)
except():
    pass

try:
    if __IPYTHON__:
        # this is used for debugging purposes only. allows to reload classes
        # when changed
        get_ipython().magic('load_ext autoreload')
        get_ipython().magic('autoreload 2')
except NameError:
    pass

import caiman as cm
from caiman.motion_correction import MotionCorrect
from caiman.source_extraction.cnmf import cnmf as cnmf
from caiman.source_extraction.cnmf import params as params
from caiman.utils.utils import download_demo
from caiman.utils.visualization import plot_contours, nb_view_patches, nb_plot_contour
bpl.output_notebook()

#%% List the available files
path = r'C:\Users\wanglab\caiman_data\mySampleData\JB44_20200123_session01sub'
#Get all the file names
files = sorted([(d,f) for d,_,fs in os.walk(path) for f in fs if f.endswith('.tif')])
fnames = [os.path.join(*i) for i in files]

#%% Display movie of data with downsampling
display_movie = True
if display_movie:
    m_orig = cm.load_movie_chain(fnames)
    ds_ratio = 0.2
    m_orig.resize(1, 1, ds_ratio).play(
        q_max=99.5, fr=100, magnification=2)

#%% Motion correction using NormCorre; params
#Params
max_shifts = (6, 6)  # maximum allowed rigid shift in pixels (view the movie to get a sense of motion)
strides =  (48, 48)  # create a new patch every x pixels for pw-rigid correction
overlaps = (24, 24)  # overlap between pathes (size of patch strides+overlaps)
num_frames_split = 100  # length in frames of each chunk of the movie (to be processed in parallel)
max_deviation_rigid = 3   # maximum deviation allowed for patch with respect to rigid shifts
pw_rigid = False  # flag for performing rigid or piecewise rigid motion correction
shifts_opencv = True  # flag for correcting motion using bicubic interpolation (otherwise FFT interpolation is used)
border_nan = 'copy'  # replicate values along the boundary (if True, fill in with NaN)

#%% start the cluster (if a cluster already exists terminate it); run NormCorre
if 'dview' in locals():
    cm.stop_server(dview=dview)
c, dview, n_processes = cm.cluster.setup_cluster(
    backend='local', n_processes=None, single_thread=False)

# create a motion correction object
mc = MotionCorrect(fnames, dview=dview, max_shifts=max_shifts,
                  strides=strides, overlaps=overlaps,
                  max_deviation_rigid=max_deviation_rigid, 
                  shifts_opencv=shifts_opencv, nonneg_movie=True,
                  border_nan=border_nan)

# correct for rigid motion correction and save the file (in memory mapped form)
mc.motion_correct(save_movie=True)
    
#%% Inspecting the effects of motion correction, rigid first
# load motion corrected movie
m_rig = cm.load(mc.mmap_file)
bord_px_rig = np.ceil(np.max(mc.shifts_rig)).astype(np.int)
#visualize original image and templates
plt.figure(figsize = (20,10))
imhand = plt.imshow(np.mean(m_orig,axis=0),cmap='gray')
imhand.set_clim(0,30)
plt.title('mean image, original')
plt.figure(figsize = (20,10))
imHand = plt.imshow(mc.total_template_rig, cmap = 'gray')
imHand.set_clim(0,30)
plt.title('mean image, rigid registration')

#inspect movie
m_rig.resize(1, 1, ds_ratio).play(
    q_max=99.5, fr=30, magnification=2, bord_px = 0*bord_px_rig) # press q to exit

#plot rigid shifts
#plt.close()

plt.figure(figsize = (20,10))
plt.plot(mc.shifts_rig)
plt.legend(['x shifts','y shifts'])
plt.xlabel('frames')
plt.ylabel('pixels')