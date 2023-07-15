from __future__ import print_function, division
import numpy as np
# from scipy.signal import find_peaks
import sys
import ROOT

# Na-22 gamma ray energy.
GAMMA_E = 511 # keV

def ROOT_peaks(h, width=10, height=0.05, options=""):
    """
    Finds peaks in hisogram `h`. `height` is measured as a fraction of the
    highest peak.  Peaks lower than `<highest peak>*height` will not be
    searched for.

    Returns an array of the peak locations, sorted in charge order, lowest to
    highest `x_pos`, and the location of the highest peak:
    (`x_pos`, `highest_peak`)

    See ROOT TSpectrum documentation for peak finding details:
    https://root.cern.ch/root/htmldoc/guides/spectrum/Spectrum.html#processing-and-visualization-functions
    """
    spec = ROOT.TSpectrum()
    highest_peak = None
    n_pks = spec.Search(h, width, options, height)               
    x_pos = spec.GetPositionX()
    x_pos = np.array([x_pos[i] for i in range(n_pks)])
    if len(x_pos) != 0:
        highest_peak = x_pos[0]
    ind = np.argsort(x_pos)
    x_pos = x_pos[ind]
    return (x_pos, highest_peak)

def fit_511(h):
    """
    511 Peak Finding Strategy
        
    We fit the highest peak with a gaussian, taking the mean of that gaussian
    as the 511 charge.

    Old Strategy: (Code commented out below. This was used when we were setting
    the trigger level too low)

    1. We use ROOT's histogram peak finding algorithm to search
       for peaks in the 511 histogram (see the method `peaks` in this
       module).
    
    2. We assume that there will always be at least two peaks
       found. We iterate through the peaks in charge order, starting
       with the largest charge.
    
    3. We fit each peak with a gaussian. The first peak we find
       that is above a threshold proportioanl to the number of
       entries is the one we take as the 511 peak. Most of the time,
       this just means we're taking the peak with the largest charge.
    """
    win = 100
    # FIXME: May want to tweak these peak finding parameters. Having `width=10`
    # seems too small, but if it's made bigger, the peak-finding algorithm
    # struggles to find the 511 keV peak. 
    # `peak` is the charge of the tallest peak in the spectrum.
    peak = ROOT_peaks(h, width=10, height=0.05, options="nobackground")[1]
    
    # `ROOT_peaks` should find at least one peak, ideally the 511 keV peak.
    if peak == None:
        return None

    # Gaussian + linear background
    f = ROOT.TF1(f"{h.GetName()}_fit",f"[2]*exp(-0.5*((x-[0]*{GAMMA_E})/[1])**2 + [3]*x + [4])", 0, 1000)
    
    f.SetParameter(0, peak/GAMMA_E)
    f.SetParameter(1, win/2)
    f.SetParLimits(1, 0, 1e9)
    f.SetParameter(2, h.GetBinContent(h.FindBin(peak)))
    
    r = h.Fit(f, 'ILMSRB', '', peak-win, peak+win)
    h.Write()
    # Secondary fit that limits the range to +/-1 sigma. This makes the
    # Gaussian + linear background approximation more accurate. 
    r = h.Fit(f, 'ILMSRB', '', GAMMA_E*f.GetParameter(0) - f.GetParameter(1), GAMMA_E*f.GetParameter(0) + f.GetParameter(1))
    f.Write()
    h.Write()
    try:
        if not r.Get().IsValid():
            return None
    except Exception as e:
        return None
    
    # x_pos = ROOT_peaks(h, width=2, height=0.05, options="nobackground")[0] 
    # print("Peak charge full list:")
    # print(x_pos)
    # f = None
    # for i in range(len(x_pos)-1, -1, -1):
    #     peak = x_pos[i]
    #     if peak < x_pos[0] + 0.01*h.GetStdDev():
    #         # All the remaining peaks will be closer to `x_pos[0]`.
    #         break
    #     print(f'Fitting this peak! {peak}')
    #     f = ROOT.TF1("f","gaus", peak-win, peak+win)
    #     r = h.Fit(f, 'ILMSR+')
    #     r = r.Get()
    #     if f.GetParameter(1) < x_pos[0]:
    #         # Impossible that this is the 511 peak 
    #         f = None
    #         continue
    #     if not r.IsValid() or f.GetParameter(2) > 150 or np.abs(x_pos[i] - f.GetParameter(1)) > 50:
    #         # Not impossible that this is the 511 peak, but there might be a
    #         # better peak later in the loop
    #         continue
    #     else:
    #         # Found the 511 peak!
    #         break
    
    return [f.GetParameter(i) for i in range(5)], [f.GetParError(i) for i in range(5)]
