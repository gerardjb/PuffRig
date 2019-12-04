'''
Building a database
#20191106
'''
import plotly
print 'plotly.__version__=', plotly.__version__  # version >1.9.4 required
import plotly.graph_objs as go

import pandas
import numpy as np
import os.path
import glob
import ntpath
import re

class eyeblinkAnalysis():
    def __init__(self):
        print 'construct eyeblinkAnalysis'
        self.folder = ''
        self.list = ''
        self.dbfile = ''
        
    def assignfolder(self, folder):
        '''folder ends in "/"'''
        print 'eyeblinkAnalysis.assignfolder', folder
        if os.path.exists(folder) :
            self.folder = folder
        else:
            print 'ERROR: eyeblinlkAnalysis.assignFolder() got bad path:', folder
            
    def builddb(self, sessionStr):
        '''build a db'''
        '''this is assuming the data format for ALL files in folder are same'''
        '''todo: add a file version to header and check if this is the case'''
        
        #open db file
        writefile = 'eyeblinkdb.csv'
        if sessionStr:
            writefile = 'eyeblinkdb_' + sessionStr + '.csv'
        dbFile = self.folder + writefile
        print 'writing dbfile:', dbFile, 'sessionStr:', sessionStr
        dbID = open(dbFile, 'w')
        
        numFiles = 0
        firstfile = 1
        rowIdx = 0
        for root, subdirs, files in os.walk(self.folder):
            #print 'root:', root
            #print '\tsubdirs:', subdirs
            #print '\t\tfiles:', files
            #print 'root:', root
            for filename in files:
                if filename.endswith('.txt'):
                    session = re.search('_s(\d+)',filename)
                    sessionName = str(session.group(1)) # session name taken from session number in filename
                    print sessionName
                    if sessionStr and not (sessionName.find(sessionStr) >= 0):
                        print '      rejecting:', sessionName
                        continue
                    #else:
                    #   print '   accepting', sessionName
                    file_path = os.path.join(root, filename)
                    with open(file_path, 'r') as f:
                        numFiles += 1
                        header = f.readline()
                        #
                        #print header
                        if firstfile:
                            dbID.write('Idx' + ',')
                            dbID.write('Session' + ',')
                            for nv in header.split(';'):
                                #print nv
                                if nv.find('=')>=0:     
                                    k, v = nv.split('=')
                                    dbID.write(k + ',')
                            dbID.write('file_path' + ',')
                            dbID.write('\n')
                            firstfile = 0
                        #
                        #write values
                        dbID.write(str(rowIdx) + ',') #Idx
                        dbID.write(sessionName + ',') #sessionName
                        for nv in header.split(';'):
                            #print nv
                            if nv.find('=')>=0:     
                                k, v = nv.split('=')
                                #print k, v
                                dbID.write(v + ',')
                        dbID.write(file_path + ',') 
                        dbID.write('\n')        
                        rowIdx += 1
        dbID.close()
        print 'db has', numFiles
        return writefile
            
    def getlist(self):
        '''return a list of files'''
        print 'eyeblinkAnalysis.getlist', self.folder
        dPathList = glob.glob(self.folder + '*.txt')
        dFileList = []
        for path in dPathList:
            dFileList.append(os.path.basename(path))
        
        #print 'eyeblinkAnalysis.getlist() dFileList=', dFileList
        theRet = ''
        for file in dFileList:
            #theRet += '"' + file + '"' + ","
            theRet += file + ","
            
        self.list = theRet
        
        return theRet
        
    def loadheader(self, file):
        print 'eyeblinkAnalysis.loadheader()', self.folder + file
        fileID = open(self.folder + file, 'r')
        header = fileID.readline() # ';' seperated list of k=v
        return header

    def plottrialparams(self, trialDict):
        ''' - plot trial parameters. use this as we are building a trial with main interface {numTrial, trialDur,...}
            - i will have another version of this to do exactly the same from a trial file
        '''
        print 'plottrialparams()'
        print 'trialDict:', trialDict
        numTrial = long(trialDict['numTrial'])
        trialDur = long(trialDict['trialDur'])

        interTrialIntervalLow = long(trialDict['interTrialIntervalLow'])
        interTrialIntervalHigh = long(trialDict['interTrialIntervalHigh'])
        preCSdur = long(trialDict['preCSdur'])
        CSdur = long(trialDict['CSdur'])
        USdur = long(trialDict['USdur'])
        CS_USinterval = long(trialDict['CS_USinterval'])
        percentUS = long(trialDict['percentUS'])
        percentCS = long(trialDict['percentCS'])
        useMotor = trialDict['useMotor'] #{motorOn, motorLocked, motorFree}
        motorSpeed = long(trialDict['motorSpeed'])
        
        print 'plottrialparams() useMotor:', useMotor
        
        totalDur = trialDur
        totalDurSec = totalDur / 1000
        
        #
        # build a square for each {preCS, postCS}
        myShapes = []
        myShapes.append(
            {
                'type': 'rect',
                'xref': 'x',
                'yref': 'paper',
                'x0': 0,
                'y0': 0,
                'x1': preCSdur,
                'y1': 1,
                'fillcolor': '#888888',
                'opacity': 0.6,
                'line': {
                    'width': 0, 
                },
            }
        )
        myShapes.append(
            {
                'type': 'rect',
                'xref': 'x',
                'yref': 'paper',
                'x0': preCSdur + CSdur,
                'y0': 0,
                'x1': totalDur,
                'y1': 1,
                'fillcolor': '#888888',
                'opacity': 0.6,
                'line': {
                    'width': 0, 
                },
            }
        )
        csStart = preCSdur
        csStop = preCSdur + CSdur
        myShapes.append(
            {
                'type': 'rect',
                'xref': 'x',
                'yref': 'paper',
                'x0': csStart,
                'y0': 0,
                'x1': csStop,
                'y1': 1,
                'fillcolor': '#FF8888',
                'opacity': 0.4,
                'line': {
                    'width': 0,
                }
            }
        )
        usStart = preCSdur + CS_USinterval
        usStop = usStart + USdur
        myShapes.append(
            {
                'type': 'rect',
                'xref': 'x',
                'yref': 'paper',
                'x0': usStart,
                'y0': 0,
                'x1': usStop,
                'y1': 1,
                'fillcolor': '888FFF',
                'opacity': 0.4,
                'line': {
                    'width': 0,
                }
            }
        )

        #print 'myShapes:', myShapes
        
        #
        layout = {
    
            'title': '',
            'xaxis': {
                #'range': [0, totalDur],
                'range': [0, totalDur],
                'autotick':True,
            },
            'yaxis': {
                #'range': [0, 5],
                'showgrid': False,
                'ticks': '',
                'showticklabels': False
            },
            'width': 500,
            'height': 250,
            'shapes': myShapes,
        }
        trace0 = go.Scatter(
            x = totalDurSec,
            y = 222,
            mode = 'lines+markers',
            name = 'this_trial'
        )
        
        #
        data = [trace0]
        #data = [trace0, trace1, trace2, trace3]
        
        fig = {
            'data': data,
            'layout': layout,
            #'config': {'displayModeBar': False}
        }

        output_type = 'div'
        
        #{displaylogo: false}
        #{displayModeBar: false}
        #fileordiv = plotly.offline.plot(fig, filename='templates/yyy.html', output_type=output_type, auto_open=False)
        #fileordiv = plotly.offline.plot(fig, filename='templates/yyy.html', output_type=output_type, auto_open=False)
        fileordiv = plotly.offline.plot(fig, show_link=False, filename='templates/yyy.html', output_type=output_type, auto_open=False)

        return fileordiv
        
        
if __name__ == '__main__':
    folder = '/home/pi/stepOrig/treadmill/data'

    t = eyeblinkAnalysis()
    t.assignfolder(folder)
    print t.getlist()
