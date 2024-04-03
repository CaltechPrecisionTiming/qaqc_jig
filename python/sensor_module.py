'''sensor_module Class
'''
import numpy as np
import math
import pandas as pd

import ROOT as rt
from ROOT import TFile, TNtuple, TTree, RDataFrame
from ROOT import TCanvas, TLegend, TLatex, TLine, TBox
from ROOT import TH1D, TH2D, TGraph, TGraphErrors
from ROOT import TF1, TMath
import json
import os
from datetime import datetime

class sensor_module:
    sources = ['lyso', 'sodium', 'cesium', 'cobalt', 'source'] #'source' included to account for data taking with weird naming bug
    channels = np.arange(32)
    ATTENUATION_FACTOR = 5.85
    SOURCE_ENERGY_FIT_PAR_IDXS = {
    'spe': (1, 3, 5),
    'lyso': (1, 3, 5),
    'sodium': (511, 0, 1),
    'cesium': (662, 0, 1),
    'cobalt': (122, 0, 1)
    } 
    path_to_quickdraw = ''
    path_to_jig_calibration = "jig_calibration_4modules_withErrors.csv"

    def __init__(self, fname: str=None, id: int=None, ov: float=None, tt: int=None, source: str=None, n_spe: int=None, n_src: int=None, temps: list=None, rotated = False, jig_calibrate = True, json_fname: str=None) -> None:
        '''Module
        loads a root file with the stored RDF and integrations
        contains the methods used to analyze and perform QAQC on a module
        
        If the fields aren't specified in constructor, file name should have the form .../module_<ID>_Vov<ov>_Vtt<tt>_Nspe<n_spe>_N<source><n_src>.root
        This is to match the output form of the QAQC GUI
        If not, certain fields will still evaluate to None, and errors may be thrown!
        '''
        ### First, define basic attributes of sensor_module. 
        ### These should be specified at initialization, or can be determined by parsing input file name
        # if we initialize with a '.root' file, we will compute all of the necessary attributes using the helper methods
        # a json file will be generated and stored at json_fname
        print("Defining sensor_module object")
        self.data_time = str(datetime.now())
        self.fname = fname 
        if ".root" in self.fname:
            
            if '/' in fname: #for ov/tt parsing, only consider file name, not full path
                fname_noPath = fname[fname.rindex('/', 0, len(fname)-1)+1:len(fname)]
                if fname_noPath[len(fname_noPath)-1]=='/':
                    fname_noPath = fname_noPath[:len(fname_noPath)-1]
            else:
                fname_noPath = fname
            #print("fname_noPath: ", fname_noPath)

            #extract module ID - should be between first and second underscores
            if id==None and "module_" in fname_noPath:
                first_underscore = fname_noPath.index('_')
                second_underscore = fname_noPath.index('_', first_underscore+1, len(fname_noPath))
                self.id = int(fname_noPath[first_underscore+1:second_underscore])
            else:
                self.id = id
            #print(self.id)

            #if tt or TT is in file name, we extract it out (module_XXXXXX_.Vov<Y.YY>.._Vtt<X.XX>)
            #we try to get the X.XX part - we assume it is in mV
            if tt == None and 'Vtt' in fname_noPath:
                ttIndex = fname_noPath.index('tt')
                self.tt = float(fname_noPath[ttIndex+2:fname_noPath.find('_', ttIndex, len(fname_noPath))])
            elif tt == None and 'VTT' in fname_noPath:
                ttIndex = fname_noPath.index('tt')
                self.tt = float(fname_noPath[ttIndex+2:fname_noPath.find('_', ttIndex, len(fname_noPath))])
            else:
                self.tt = tt #will be set to passed value, otherwise it will be None
            if self.tt!=None and float(self.tt)>0: # convert to volts with negative threshold
                self.tt = float(self.tt)/(-1000)
            
            #do the same for ov as tt
            if ov == None and 'Vov' in fname_noPath:
                ovIndex = fname_noPath.index('ov')
                self.ov = float(fname_noPath[ovIndex+2:fname_noPath.find('_', ovIndex, len(fname_noPath))])
            elif ov == None and 'VoV' in fname:
                ovIndex = fname_noPath.index('oV')
                self.ov = float(fname_noPath[ovIndex+2:fname_noPath.find('_', ovIndex, len(fname_noPath))])
            else:
                self.ov = ov #will be set to passed value, otherwise it will be None
            #print(self.ov)
            #extract out number of source name
            self.jig_calibrate = bool(int(jig_calibrate))
            if source not in sensor_module.sources and source!=None:
                raise RuntimeError("Invalid Source Specified! Allowed Sources are 'lyso', 'sodium', 'cesium', 'cobalt', and 'source'")
            self.source = source
            if source==None:
                for source in sensor_module.sources:
                    if source in fname_noPath:
                        self.source = source
                        break
                
            #print(self.source)
            #extract out number of source events
            if n_src == None and source != None and "N"+source in fname_noPath:
                src_index = fname_noPath.rindex(self.source)
                root_index = fname_noPath.rindex(".root") #from file name template, n_src should be right at the end
                self.n_src = int(fname_noPath[src_index+len(self.source): root_index])
            else:
                self.n_src = n_src
            #print(self.n_src)
            
            #extract out number of spe events
            if n_spe == None and "Nspe" in fname_noPath:
                spe_index = fname_noPath.rindex("Nspe")
                underscore_index = fname_noPath.find('_', spe_index, len(fname_noPath))
                self.n_spe = int(fname_noPath[spe_index+4:underscore_index])
            else:
                self.n_spe = n_spe
            #print(self.n_spe)
            
            self.temps = temps
            self.rotated =bool(int(rotated))
            
            ### Now, we compute fit parameters, LYSO arrays, and so forth using helper methods
            #store dictionary with channel-by-channel fit information for SPE hists
            #this information will be from hists outputtted by "analyze-waveforms" 
            self.spectra_params_spe = self.get_spectra_params_spe(fname, self.jig_calibrate)
            #print(self.spectra_params_spe)
            self.spectra_params_src = self.get_spectra_params_src(fname, self.source, self.jig_calibrate)
            #print(self.spectra_params_src)
            
            
            ### compute LY array information by bar, separated by spe, src, and pe
            #store dictionary with LY Info by bar, in the form [(left LY, left LY error), (avg LY, avg LY error), (right LY, right LY error)]
            #note that this is effectively the same as the "spectra params" fields, except now it is organized by bar instead of channel, and includes information about average LY across bvar
            self.ly_spe = self.get_LY_dict(self.spectra_params_spe, "spe")
            self.ly_src = self.get_LY_dict(self.spectra_params_src, "src")
            self.ly_pe = self.get_LY_dict_pe(self.ly_spe, self.ly_src)
            
            
            #compute LY RMS for spe, src, and pe. Each is an array with three components: [LY RMS left side, LY RMS average, LY RMS right side]
            self.ly_rms_spe = self.get_LY_rms(self.ly_spe, "spe")
            self.ly_rms_src = self.get_LY_rms(self.ly_src, "src")
            self.ly_rms_pe = self.get_LY_rms(self.ly_pe, "pe")

            #print(self.ly_rms_spe, self.ly_rms_src, self.ly_rms_pe)

            #compute difference in LY between the two sides of the bar as a fraction of the average LY for that bar (left LY-right LY)/(left LY + right LY)
            #this will be done on a bar-by-bar basis, and the average of the absolute differences across the bars: abs((left LY-right LY)/(left LY + right LY)) averaged over all 16 bars
            #we do this for spe, src, and pe
            self.ly_difference_spe_arr = list(self.get_LY_Difference_arr(self.ly_spe, "spe"))
            self.ly_difference_src_arr = list(self.get_LY_Difference_arr(self.ly_src, "src"))
            self.ly_difference_pe_arr = list(self.get_LY_Difference_arr(self.ly_pe, "pe"))
            #print(self.ly_difference_spe_arr, self.ly_difference_src_arr, self.ly_difference_pe_arr)

            self.ly_difference_spe_avg = np.average(np.abs(self.ly_difference_spe_arr))
            self.ly_difference_src_avg = np.average(np.abs(self.ly_difference_src_arr))
            self.ly_difference_pe_avg = np.average(np.abs(self.ly_difference_pe_arr))
            #print(self.ly_difference_spe_avg, self.ly_difference_src_avg, self.ly_difference_pe_avg)

            #now, call function to store fields in json
            self.store()
        
        # if we intialize with .json file that exists, we will define the class attributes by reading from the file,
        # NOT from recomputing everything using the class helper methods
        elif ".json" in self.fname and os.path.isfile(self.fname):
            attributes_dict = self.load()
            for key in attributes_dict.keys():
                setattr(self, key, attributes_dict[key])

        
        
        
        
        else:
            raise RuntimeError("ERROR: either the input file name does not have the .root or .json extension, or it does not exist!") 
        
        
        
        
        
        '''
        self.ov = ov # V
        self.n_spe = 100_000
        self.n_src = 200_000
        

        self.stats = self.load_json()
        if self.stats["ly_rms"] is None: # example
            self.ly_rms = self.ly_function()
            self.store_json()
        '''
    def __getitem__(self, key): # example
        return self.stats[key]
    
    # **** #
    def store(self, filename: str=None): # example
        "Generate JSON file with all 'sensor_module' fields stored in dictionary"
        print("Generating json output")
        if filename==None:
            filename = self.fname.replace('.root', '.json')
        with open(filename, "w") as outfile:
            json.dump(self.__dict__, outfile)
        return

    def load(self): # example
        json_fname = self.fname.replace('.root', '.json')
        with open(json_fname, 'r') as infile:
            attributes_dict = json.load(infile)
        return attributes_dict

    # **** #
    def run_analysis(self): # example
        ''''''
        pass

    def rotate(self) -> None:
        '''rotate the channel mappings'''
        pass

    def calibrate(self, factors: dict):
        '''applies the calibration factors to the spe and source data'''
        pass
    
    def fit_spectra(self, hist): # spe, src
        '''returns fit parameters for hist provided
            fits by Paul
        '''

        def gaus(x, mu=0, sig=1):
            return 1/(sig*(2*math.pi)**0.5) * math.exp(-0.5*((x-mu)/sig)**2)
        def bc(bin):
            return hist.GetBinCenter(bin)
        def bv(x):
            return hist.GetBinContent(hist.FindBin(x))
        NS = 2
        for i in range(3):
            if i == 0:
                # bin = 0;       underflow bin
                # bin = 1;       first bin with low-edge xlow INCLUDED
                # bin = nbins;   last bin with upper-edge xup EXCLUDED
                # bin = nbins+1; overflow bin
                centers = np.array([hist.GetBinCenter(ibin) for ibin in range(1, hist.GetNbinsX())])
                content = np.array([hist.GetBinContent(ibin) for ibin in range(1, hist.GetNbinsX())])
                A, mu = 0, 0
                for ic, (ce, co) in enumerate(zip(centers[::-1], content[::-1])):
                    if ic < 10:
                        continue
                    # co_ave10, ce_ave10 = np.mean(content[::-1][ic-5:ic+5]), np.mean(centers[::-1][ic-5:ic+5])
                    # co_ave10, ce_ave10 = np.mean(content[::-1][ic-10:ic+10]), np.mean(centers[::-1][ic-10:ic+10])
                    # if co_ave10 > A and co_ave10 > np.mean(content):
                    #     mu, A = ce_ave10, co_ave10
                    # if mu and co_ave10 < A*3/4:
                    #     break
                    left = np.mean(content[::-1][ic:ic+15])
                    right = np.mean(content[::-1][ic-15:ic])
                    center = np.mean(content[::-1][ic-5:ic+5])
                    if center > left and center > right and center > np.mean(content) and abs(left-right)*2/(left+right) < 0.1:# and center > A:
                        mu, A = ce, co
                        # print(hist.GetName(), mu, A)
                        break
                # idx = np.argmax(content * (np.abs(centers-mu) < 10))
                # A, mu = content[idx], centers[idx]
                # A, mu = hist.GetMaximum(), bc(hist.GetMaximumBin())
                # A = hist.GetBinContent(int(mu))
                sig, p0, p1, p2 = 0, 0, 0, 0



            # print(f'{A=:.2f}, {mu=:.2f}, {sig=:.2f}, {p0=:.2f}, {p1=:.2f}')
            # aa = A*gaus(0)/2+p0+p1*(mu+sig*(2*math.log(2))**0.5)
            aa = A/2+p0+p1*(mu+sig*(2*math.log(2))**0.5)
            x_fwhm = bc(hist.FindLastBinAbove(aa))
            fwhm = abs((x_fwhm-mu)) * 2
            # fwhm = bc(hist.FindLastBinAbove(aa)) \
            #         - bc(hist.FindFirstBinAbove(aa,
            #                                     firstBin=hist.FindBin(mu - (1.5*(bc(hist.FindLastBinAbove(aa)) - mu))),
            #                                     lastBin=hist.GetMaximumBin()))
            sig = fwhm / (2*(2*math.log(2))**0.5)
            # if not sig:
            #     sig = mu**0.5

            # print(bv(mu),bv(mu-3*sig),bv(mu+3*sig))
            # A = gaus(0) * (bv(mu) - (bv(mu-NS*sig)+bv(mu+NS*sig))/2) / ( gaus(0) - gaus(NS) )
            A = gaus(0) * (bv(mu) - (bv(mu-NS*sig)+bv(mu+NS*sig))/2) / ( gaus(0) - gaus(NS) )
            # print(i)
            p1 = (bv(mu+NS*sig)-bv(mu-NS*sig)) / (2*NS*sig)
            p0 = bv(mu) - (A + p1*mu)
            # print(i, hist.GetName(), mu, sig, A, p0, p1)
            # print(f'{A=:.2f}, {mu=:.2f}, {sig=:.2f}, {p0=:.2f}, {p1=:.2f}')

            # hist.hh.GetListOfFunctions()[-1].SetParameter(0, A)
            # hist.hh.GetListOfFunctions()[-1].SetParameter(0, A)
        # print()

        # A *= gaus(0)
        for source in sensor_module.SOURCE_ENERGY_FIT_PAR_IDXS:
            if source in hist.GetName():
                eng = sensor_module.SOURCE_ENERGY_FIT_PAR_IDXS[source][0]
                mu /= eng
        # print(hist.GetListOfFunctions()[-1].GetFormula())
        # mu0 = hist.GetListOfFunctions()[-1].GetParameter(0)
        # sig0 = hist.GetListOfFunctions()[-1].GetParameter(1)
        # A0 = hist.GetListOfFunctions()[-1].GetParameter(2)
        # print(f'{hist.GetName()}')
        # print(f'{mu0=:.2f}, {mu=:.2f} | {(mu-mu0)/mu0=:.3f}')
        # print(f'{sig0=:.2f}, {sig=:.2f} | {(sig-sig0)/sig0=:.3f}')
        # print(f'{A0=:.2f}, {A=:.2f} | {(A-A0)/A0=:.3f}')
        # print()

        offset = 0 # also will need to fix
        fit_eq = f'[2]*exp(-0.5*(x-{offset}-[0]*{eng})**2/[1]**2) + [3] - [4]*x'# + [5]*x*x'

        # print(mu, sig, A, p0, p1)
        for i in range(3):
            xl, xh = mu*eng - NS*sig, mu*eng + NS*sig
            fit = TF1(f'{hist.GetName()}_fit{i}',fit_eq, xl, xh)
            fit.SetParLimits(0, mu*2/3, mu*4/3)
            fit.SetParLimits(1, sig*2/3, sig*4/3)
            fit.SetParLimits(2, A*2/3, A*4/3)
            # fit.SetParLimits(3, p0*2/3, p0*4/3)
            fit.SetParLimits(3, 0, p0*4/3)
            # fit.SetParLimits(4, p1*2/3, p1*4/3)
            fit.SetParLimits(4, 0, p1*4/3)

            fit.SetParameters(mu, sig, A, p0, p1)#, p2)
            # fit.SetParLimits(4, -1e999, 0)
            # fit.SetRange(xl, xh)
            hist.Fit(fit, 'QMN', '', xl, xh)
            hist.GetListOfFunctions()[-1] = fit
            mu = fit.GetParameter(0)
            sig = fit.GetParameter(1)
            A = fit.GetParameter(2)
            p0 = fit.GetParameter(3)
            p1 = fit.GetParameter(4)
            # p2 = fit.GetParameter(5)
            # print(mu*eng, sig, A, p0, p1)
        # print('\n\n')
        new_chisq = fit.GetChisquare()
        # print(f', {new_chisq}],') # @FIT_X2
        return mu, fit.GetParError(0), sig, A, p0, p1

        

    def get_spectra_params_src(self, inputFile: str=None, source: str=None, calibrate: bool=True):
        '''returns dictionary with fit parameters for each channel from source spectra'''
        if inputFile==None:
            inputFile==self.fname
        if inputFile==None and ".root" not in inputFile:
            raise RuntimeError("No Input File Specified with Histograms")
        
        if source not in sensor_module.sources:
            raise RuntimeError("Source is not recognized")
        
        tfile = TFile(inputFile)
        spectra_params_dict = {}
        for channel in sensor_module.channels:
            hist = tfile.Get(f'{source}_ch{channel}')
            mu, mue, sig, A, p0, p1 = self.fit_spectra(hist)
            if calibrate and os.path.exists(sensor_module.path_to_jig_calibration):
                calibration_data = pd.read_csv(sensor_module.path_to_jig_calibration, delimiter=',')
                mu_cal = calibration_data.iloc[1][f'ch{channel}']*mu
                mue_cal = mu_cal*((calibration_data.iloc[4][f'ch{channel}']/calibration_data.iloc[1][f'ch{channel}'])**2+(mue/mu)**2)**0.5
            else:
                mu_cal = mu; mue_cal = mue
            spectra_params_dict[f"ch{channel}"] = (mu_cal*1000*sensor_module.ATTENUATION_FACTOR, mue_cal*1000*sensor_module.ATTENUATION_FACTOR)
            
        return spectra_params_dict                 

    def get_spectra_params_spe(self, inputFile: str=None, calibrate: bool=True):
        '''returns dictionary with fit parameters for each channel from spectra
        we use a different function for spe params because we do not re-fit after Tony's 
        original fits, like Paul does for source spectra
        '''
        if inputFile==None:
            inputFile==self.fname
        if inputFile==None and ".root" not in inputFile:
            raise RuntimeError("No Input File Specified with Histograms")
        
        tfile = TFile(inputFile)
        spectra_params_dict = {}
        for channel in sensor_module.channels:
            fit = tfile.Get(f'spe_ch{channel}_fit') # will extract single spe charge + uncertainty directly from fit
            mu, mue = fit.GetParameter(3), fit.GetParameter(5)
            if calibrate and os.path.exists(sensor_module.path_to_jig_calibration):
                calibration_data = pd.read_csv(str(sensor_module.path_to_jig_calibration), delimiter=',')
                mu_cal = calibration_data.iloc[0][f'ch{channel}']*mu #spe calibration values at row 0
                mue_cal = mu_cal*((calibration_data.iloc[3][f'ch{channel}']/calibration_data.iloc[0][f'ch{channel}'])**2+(mue/mu)**2)**0.5 #spe error at row 3
            else:
                mu_cal = mu; mue_cal = mue
            spectra_params_dict[f"ch{channel}"] = (mu_cal, mue_cal)
        return spectra_params_dict   


    def get_LY_dict(self, spectra_dict: dict=None, LY_type: str="src"):
        '''returns dictionary with either spe charge or charge from source events. Each key is a bar, and each value is [[left side LY, err]
        [average bar LY, err], [right side LY, err]]'''
        if LY_type not in ['spe', 'src']:
            raise RuntimeError("Invalid LY type specified! Should be 'spe' or 'src'.")
        if spectra_dict == None and LY_type == "spe":
            spectra_dict = self.get_spectra_params_spe(self.fname)
        if spectra_dict == None and LY_type == "src":
            spectra_dict = self.get_spectra_params_src(self.fname, self.source)
        spectra_info = np.array(list(spectra_dict.values()))
        lys = [spectra_info[:16],(spectra_info[:16]+spectra_info[16:])/2,spectra_info[16:]]
        ly_dict = {}
        for barNum in range(16):
            barArr = []
            for x in range(3): #loop over left, average,and right side of bar
                barArr.append(lys[x][barNum][:].tolist())
            ly_dict[f"bar{barNum}"] = barArr
        return ly_dict

    def get_LY_dict_pe(self, ly_spe_dict: dict=None, ly_src_dict: dict=None):
        '''returns dictionary with number of photoelectrons from source events (normalized by SPE charge).
        Each key is a bar, and each value is [[left side LY, err]
        [average bar LY, err], [right side LY, err]]'''
        if ly_spe_dict==None:
            ly_spe_dict = self.get_LY_array(None, LY_type="spe")
        if ly_src_dict==None:
            ly_src_dict = self.get_LY_array(None, LY_type="src")
        ly_pe_dict = {}
        for barNum in range(16):
            barArr = []
            for x in range(3): #loop over left, average , and right side of bar
                pe_val = ly_src_dict[f"bar{barNum}"][x][0]/ly_spe_dict[f"bar{barNum}"][x][0]
                barArr.append([pe_val, pe_val*((ly_src_dict[f"bar{barNum}"][x][1]/ly_src_dict[f"bar{barNum}"][x][0])**2+(ly_spe_dict[f"bar{barNum}"][x][1]/ly_spe_dict[f"bar{barNum}"][x][0])**2)**0.5])
            ly_pe_dict[f"bar{barNum}"] = barArr
        
        return ly_pe_dict

    def get_LY_rms(self, ly_dict: dict=None, LY_type: str=None):
        if LY_type not in ['spe', 'src', 'pe']:
            raise RuntimeError("Invalid LY type specified! Should be 'spe', 'src', or 'pe'.")
        if ly_dict == None and LY_type=='pe':
            ly_dict = self.get_LY_dict_pe(self.get_LY_dict(LY_type="spe"), self.get_LY_dict(LY_type="src"))
        if ly_dict == None and LY_type in ['spe', 'src']:
            ly_dict = self.get_LY_dict(LY_type = LY_type)
        ly_arr = np.array(list(ly_dict.values()))
        rms_arr = []
        for x in range(3): #loop over left, average, and right LY for bars
            rms_arr.append(ly_arr[:,x,0].std()/ly_arr[:,x,0].mean())
        return rms_arr

    def get_LY_Difference_arr(self, ly_dict: dict=None, LY_type: str=None):
        if LY_type not in ['spe', 'src', 'pe']:
            raise RuntimeError("Invalid LY type specified! Should be 'spe', 'src', or 'pe'.")
        if ly_dict == None and LY_type=='pe':
            ly_dict = self.get_LY_dict_pe(self.get_LY_dict(LY_type="spe"), self.get_LY_dict(LY_type="src"))
        if ly_dict == None and LY_type in ['spe', 'src']:
            ly_dict = self.get_LY_dict(LY_type = LY_type)
        ly_arr = np.array(list(ly_dict.values()))
        return (ly_arr[:,0,0]-ly_arr[:,2,0])/(ly_arr[:,0,0]+ly_arr[:,2,0])

    
    def plot_spectra(self, source: str='spe', outputDir = None):
        #will call quick-draw.py
        pass

    def plot_light_yield(self) -> (rt.TH1):
        pass

    def plot_crosstalk_spectra(self, channel=0, neighbors=2):
        pass

    def plot_crosstalk_matrix(self):
        #will call quick-draw.py equivalent for crosstalk
        pass

    def get_saturation_counts(self):
        pass

    def run_tests(self):
        pass
