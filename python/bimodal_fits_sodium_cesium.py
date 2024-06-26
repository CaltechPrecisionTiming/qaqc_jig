import ROOT
import numpy as np
from scipy.optimize import curve_fit
import matplotlib.pyplot as plt
from pathlib import Path

def bi_modal(x, a1, m1, s1, a2, m2, s2, m, c):
    return (
        a1 * np.exp(-(((x - m1) / s1) ** 2))
        + a2 * np.exp(-(((x - m2) / s2) ** 2))
        + m * x
        + c
    )

def find_main_peak(hist, move_param):
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
                left = np.mean(content[::-1][ic:ic+move_param])
                right = np.mean(content[::-1][ic-move_param:ic])
                center = np.mean(content[::-1][ic-5:ic+5])
                if center > left and center > right and center > np.mean(content) and abs(left-right)*2/(left+right) < 0.1:# and center > A:
                    mu, A = ce, co
                    # print(hist.GetName(), mu, A)
                    return mu, A
        return 0,0

def fit_modified(file_path, CHANNEL, source):
    fit_info = {}
    fit_info[CHANNEL] = {}
    #plt.figure(figsize=(8, 6))
    # Open the ROOT file
    file = ROOT.TFile.Open(file_path)

    # Retrieve the "sodium_ch0" tree from the file
    sodium_tree = file.Get(f"{source}_ch{CHANNEL}")
    bin_width = sodium_tree.GetBinWidth(30)

    # Get the number of entries in the tree
    num_entries = len(sodium_tree)

    sodium_tree = file.Get(f"{source}_ch{CHANNEL};1")
    num_entries = len(sodium_tree)
    sodium_data = np.zeros(num_entries)
    bins = np.zeros(num_entries)

    hard_edge = sodium_tree.GetBinCenter(0)
    for i in range(num_entries):
        sodium_data[i] = sodium_tree.GetBinContent(i)
        bins[i] = sodium_tree.GetBinCenter(i)
    
    #idx_start = sodium_tree.FindBin(200)
    # Close the ROOT file
    found_fit = False

    #to set the upper bound that we sweep over, determine maximum after 100, then divide by two, and add buffer
    #sweep in increments of 10, starting from 50 lower than upper bound
    peak_distance_param = 2
    #print(sodium_tree.FindBin(100)+1)
    for move_param in np.arange(5, 30, 5):
        mu, A = find_main_peak(sodium_tree, move_param)
        #print(mu,A)
        initial_cuts = np.arange(100, 200, 10)
        peak_position = np.argmin(abs(bins-mu))
        #for initial_cut in initial_cuts:
            #maxPosition = np.argmax(sodium_data[sodium_tree.FindBin(initial_cut)+1:])+sodium_tree.FindBin(initial_cut)+1
            #print("maxPosition: ", maxPosition)
            #print("max X Position: ", sodium_tree.GetBinCenter(int(maxPosition)))
            #print(sodium_tree.GetBinCenter(int(maxPosition)))
        end_sweep = sodium_tree.GetBinCenter(int(peak_position))/peak_distance_param
        #print("end_sweep: ", end_sweep)
        back_param = 50; buffer = 0
        if end_sweep-back_param < hard_edge:
            back_param = end_sweep - hard_edge
        if end_sweep < hard_edge:
            end_sweep = hard_edge; back_param = 0; buffer=10
            #print("back_Param =0")
        for start_bin in np.arange(end_sweep-back_param, end_sweep+buffer, 10):
            fit_info[CHANNEL][(start_bin, move_param)] = {}
            idx_start = sodium_tree.FindBin(start_bin)
            #file.Close()
            
            x = bins
            y = sodium_data
            x_original = bins
            y_original = sodium_data
        
            # smooth the data
            '''
            step = 100
            y_smooth = np.convolve(y, np.ones(step), "same") / step
            # plt.plot(x, y, label="Smoothed data")
        
            # find the first local minimum for the cutoff
            
            idx_min_1 = 0
            for i in range(idx_start, len(y_smooth)):
                if y_smooth[i] < y_smooth[i - 1] and y_smooth[i] < y_smooth[i + 1]:
                    idx_min_1 = i
                    break
            #print("idx_min_1", idx_min_1)
            
            '''
            #idx_min_1=0
            #list local maxima
            step = 40
            y_smooth_max = np.convolve(y, np.ones(step), "same") / step
            # plt.plot(x, y, label="Smoothed data")
            
            # find the first local minimum for the cutoff
            '''
            maxima = []
            for i in range(1, len(y_smooth_max)):
                if y_smooth_max[i] > y_smooth_max[i - 1] and y_smooth_max[i] > y_smooth_max[i + 1]:
                    maxima.append(i*bin_width*len(y_smooth_max)/num_entries)
                    
            #print(maxima)
            '''
            idx_end = -10
            
            
             # smooth the data
            step = 30
            y_smooth = np.convolve(y, np.ones(step), "same") / step
        
            
            idx_max_2 = peak_position
            idx_max_1 = int((idx_max_2 - idx_start) / peak_distance_param + idx_start)
        
            #idx_start = idx_max_1 - 10
            '''
            plt.axvline(x=x[idx_start], color="black", linestyle="--", label="Lower cutoff")
            plt.axvline(x=x[idx_max_1], color="blue", linestyle="--", label="Peak 1 Guess")
            plt.axvline(x=x[idx_max_2], color="red", linestyle="--", label="Peak 2 Guess")
            plt.axvline(x=x[idx_end], color="black", linestyle="--", label="Upper cutoff")
            '''
        
        
            # cut the data
            x = x[idx_start:idx_end]
            y = y[idx_start:idx_end]
            idx_max_2 -= idx_start
            idx_max_1 -= idx_start
        
            # smooth the data for the fitting (need more details)
            # step = 10
            step = 10
            y = np.convolve(y, np.ones(step), "same") / step
            #if y[idx_max_2]<num_events/1000: print("continuing"); continue;
            # Initial guess for the parameters
            #print(y[idx_max_2], x[idx_max_2])
            p0_bi = [
                # a, mean, sigma
                y[idx_max_1], x[idx_max_1], 10, 
                y[idx_max_2], x[idx_max_2], 10, 
                # m, c
                -0.01, 10]
            #print(y[idx_max_2])
            low = -10000; high = 10000
            #bounds = ([low, low, low, num_events/1000, low, low, -0.1, low],[high, high, high, high, high, high, 0, high])
            bounds = ([y[idx_max_1]/5, low, low, low, low, low, -0.1, low],[y[idx_max_1]*5, high, high, high, high, high, 0, high])
            # Perform the curve fitting
            x = x[5:]
            y = y[5:]
            try:
                popt, pcov = curve_fit(bi_modal, x, y, p0=p0_bi, bounds=bounds)
                found_fit = True
            
            except RuntimeError:
                continue 
            a1, m1, s1, a2, m2, s2, m, c = popt
        
            chi2 = np.sum((bi_modal(x, *popt) - y) ** 2)
    
            
            fit_info[CHANNEL][(start_bin, move_param)]["Param_List"] = [a1, m1, s1, a2, m2, s2, m, c]
            fit_info[CHANNEL][(start_bin, move_param)]["Chi-Squared"] = chi2/len(x)

    if not found_fit:
        print(f"Fit does not converge for channel {CHANNEL}")
        return 0,0,0,0,0,0,0,0,0
        
    min_chi2 = 100000000; bestKey = 0
    for key, value in fit_info[CHANNEL].items():
        if "Chi-Squared" not in list(value.keys()): continue;
        if value["Chi-Squared"] < min_chi2:
            min_chi2 = value["Chi-Squared"]
            bestKey = key
    
    #print(bestKey)

    #print("Best Key: ", bestKey)
    # Generate points for the fitted curve
    x_fit = np.linspace(bestKey[0], np.max(x), 1000)
    #y_fit = bi_modal(x_fit, a1, m1, s1, a2, m2, s2, m, c)
    #print(x_fit)
    y_fit = bi_modal(x_fit, *fit_info[CHANNEL][bestKey]["Param_List"])
    #print(ch, ": ",fit_info[CHANNEL][bestKey]["Param_List"])
    return fit_info[CHANNEL][bestKey]["Param_List"],fit_info[CHANNEL][bestKey]["Chi-Squared"]
