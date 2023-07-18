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
    as the 511 charge. This strategy relies on cutting crosstalk since
    otherwise the highest peak will be around 0 pC.
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
    # f = ROOT.TF1(f"{h.GetName()}_fit",f"[2]*exp(-0.5*(x-[0]*{GAMMA_E})**2/[1]**2) + [3]*x + [4]", 0, 1000)
    f = ROOT.TF1(f"{h.GetName()}_fit",f"[2]*exp(-0.5*(x-[0]*{GAMMA_E})**2/[1]**2)", 0, 1000)
    fit_options = 'LSB' 
    f.SetParameter(0, peak/GAMMA_E)
    f.SetParameter(1, win/2)
    f.SetParameter(2, h.GetBinContent(h.FindBin(peak)))
    
    r = h.Fit(f, fit_options, '', peak-win, peak+win)
    h.Write()
    # Secondary fit that limits the range to +/-1.5 sigma. This makes the model
    # more accurate. The expression includes `abs` because sometimes ROOT makes
    # sigma negative. This is not ideal, but when I tried limiting sigma to
    # positive values, ROOT had trouble fitting some histograms for unknown
    # reasons.
    r = h.Fit(f, fit_options, '', GAMMA_E*f.GetParameter(0) - 1.5*abs(f.GetParameter(1)), GAMMA_E*f.GetParameter(0) + 1.5*abs(f.GetParameter(1)))
    f.Write()
    h.Write()
    try:
        if not r.Get().IsValid():
            return None
    except Exception as e:
        return None
    # Use commented return statement for linear background model because it
    # adds 2 more parameters. 
    # return [f.GetParameter(i) for i in range(5)], [f.GetParError(i) for i in range(5)]
    return [f.GetParameter(i) for i in range(3)], [f.GetParError(i) for i in range(3)]
