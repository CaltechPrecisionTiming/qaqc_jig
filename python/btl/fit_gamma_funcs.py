from __future__ import print_function, division
import numpy as np
import sys
import ROOT

FIT_OPTIONS = 'LSB' 

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

def fit_gamma(h, eng, offset=0, offset_sigma=10):
    """
    Finds the full energy gamma peak (with energy `eng`) in the ROOT histogram
    `h`, and fits it with a Gausssian. Returns the fit parameters, with the
    first parameter being pC per keV.
    
    Also fits the peak corresponding to 0 keV events to measure any offset.
    """
    
    # Now we find the full energy peak. We don't use `GetMaximumBin` because we
    # want our estimate of the full energy peak to be sufficiently far away
    # from the offset peak.
    peak_bin = None
    for i in range(h.FindBin(offset + 3*offset_sigma), h.GetNbinsX()):
        if peak_bin is None or h.GetBinContent(i) > h.GetBinContent(peak_bin):
            peak_bin = i
    if peak_bin is None:
        return None
    peak = h.GetBinCenter(peak_bin)
    # Gaussian + linear background
    # f = ROOT.TF1(f"{h.GetName()}_fit",f"[2]*exp(-0.5*(x-{offset}-[0]*{eng})**2/[1]**2) + [3]*x + [4]", 0, 1000)
    f = ROOT.TF1(f"{h.GetName()}_fit",f"[2]*exp(-0.5*(x-{offset}-[0]*{eng})**2/[1]**2)", 0, 1000)
    sigma = 100
    f.SetParameter(0, peak/eng)
    f.SetParameter(1, sigma/2)
    f.SetParameter(2, h.GetBinContent(h.FindBin(peak)))
    
    r = h.Fit(f, FIT_OPTIONS, '', peak-sigma, peak+sigma)
    h.Write()
    # Secondary fit that limits the range to +/-1.5 sigma. This makes the model
    # more accurate. The expression includes `abs` because sometimes ROOT makes
    # sigma negative. This is not ideal, but when I tried limiting sigma to
    # positive values, ROOT had trouble fitting some histograms for unknown
    # reasons.
    r = h.Fit(f, FIT_OPTIONS, '', eng*f.GetParameter(0) - 1.5*abs(f.GetParameter(1)), eng*f.GetParameter(0) + 1.5*abs(f.GetParameter(1)))
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

def fit_offset(h):
    """
    Fits the peak corresponding to 0 keV events to measure any offset.
    
    Returns the fit parameters.
    """
    peak = h.GetBinCenter(h.GetMaximumBin())
    sigma = 10
    # FIXME: Gaussian noise model is just a guess. However, we really only need
    # the average noise, which a narrow Gaussian can roughly estimate.
    f = ROOT.TF1(f"{h.GetName()}_offset_fit", "gaus", -100, 100)
    f.SetParameter(1, peak)
    f.SetParameter(2, sigma)
    f.SetParameter(0, h.GetBinContent(h.FindBin(peak)))
    
    r = h.Fit(f, FIT_OPTIONS, '', peak-sigma, peak+sigma)
    h.Write()
    # Secondary fit that we limit to +/-1 sigma. 
    r = h.Fit(f, FIT_OPTIONS, '', f.GetParameter(1) - abs(f.GetParameter(2)), f.GetParameter(1) + abs(f.GetParameter(2)))
    f.Write()
    h.Write()

    try:
        if not r.Get().IsValid():
            return None
    except Exception as e:
        return None
    # 1: mean
    # 2: sigma
    # 0: scale
    par_order = (1, 2, 0)
    return [f.GetParameter(i) for i in par_order], [f.GetParError(i) for i in par_order]



