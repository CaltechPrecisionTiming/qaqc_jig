from __future__ import print_function, division
import numpy as np
import sys
import ROOT

FIT_OPTIONS = 'LSB' 

def ROOT_peaks(h, width=10, height=0.05, npeaks=4, options="", sort=True):
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

    h.GetXaxis().SetRangeUser(300.,2000.)
    
    spec = ROOT.TSpectrum(npeaks)
    highest_peak = None

    n_pks = spec.Search(h, width, options, height)               
    x_pos = spec.GetPositionX()
    x_pos = np.array([x_pos[i] for i in range(n_pks)])

    if sort:
        x_pos.sort()
        
    if len(x_pos) != 0:
        highest_peak = x_pos[0]
        if sort:
            highest_peak = x_pos[len(x_pos)-1]
    
    #print('found peaks: ', x_pos)
    #print('highest peak:', highest_peak)
    
    ind = np.argsort(x_pos)
    x_pos = x_pos[ind]

    h.GetXaxis().SetRangeUser(0.,2000.)
    
    return (x_pos, highest_peak)

'''
def fit_gamma(h, eng, offset=0, offset_sigma=10):
    """
    Finds the full energy gamma peak (with energy `eng`) in the ROOT histogram
    `h`, and fits it with a Gausssian. Returns the fit parameters, with the
    first parameter being pC per keV.
    """
    
    ## Use TSpectrum
    nPeaks = 5

    h.GetXaxis().SetRangeUser(offset+3*offset_sigma,h.GetBinCenter(h.GetNbinsX()-1))
    _ ,peak = ROOT_peaks(h,width=10,height=0.3,npeaks=nPeaks,options='nobackground')
    if (peak == None): peak = 100
    
    # Now we find the full energy peak. We don't use `GetMaximumBin` because we
    # want our estimate of the full energy peak to be sufficiently far away
    # from the offset peak.
    # peak_bin = None
    # for i in range(max(1, h.FindBin(offset + 3*offset_sigma)), h.GetNbinsX()):
    #     if peak_bin is None or h.GetBinContent(i) > h.GetBinContent(peak_bin):
    #         peak_bin = i
    # if peak_bin is None:
    #     return None
    # peak = h.GetBinCenter(peak_bin)

    ## Use GetMaximumBin
    #peak_bin = h.GetMaximumBin()
    #peak = h.GetBinCenter(peak_bin)

    # Use moving average
    # val_max = -999.
    # peak_bin = -1
    # for bin in range(int(h.GetNbinsX()/3.),h.GetNbinsX()+1-5):
    #     val_temp = 0.
    #     for it in range(5):
    #         val_temp += h.GetBinContent(bin+it)
    #     if val_temp > val_max:
    #         val_max = val_temp
    #         peak_bin = bin+2
    # peak = h.GetBinCenter(peak_bin)
    
    
    # Gaussian + linear background
    # f = ROOT.TF1(f"{h.GetName()}_fit",f"[2]*exp(-0.5*(x-{offset}-[0]*{eng})**2/[1]**2) + [3]*x + [4]", 0, 1000)
    print('peak for fit: ', peak, ' -- offset = ', offset, ' -- eng=', eng)
#    f = ROOT.TF1(f"{h.GetName()}_fit",f"[2]*exp(-0.5*(x-{offset}-[0]*{eng})**2/[1]**2)", 0, 1000)
    f = ROOT.TF1(f"{h.GetName()}_fit",f"[2]*exp(-0.5*(x-{offset}-[0]*{eng})**2/([1]*{eng})**2)", peak*0.8, peak*1.1)
    sigma = h.GetRMS()
    f.SetNpx(10000)
    f.SetLineColor(ROOT.kRed)
    f.SetParameter(0, (peak-offset)/eng)
    f.SetParameter(1, 0.08*peak/eng)
    f.SetParameter(2, h.GetBinContent(h.FindBin(peak)))
    r = h.Fit(f, 'Q0LSB+',  '', peak-0.15*peak, peak+0.30*peak)
    #h.Write()
    
    # Secondary fit that limits the range to +/-1.5 sigma. This makes the model
    # more accurate. The expression includes `abs` because sometimes ROOT makes
    # sigma negative. This is not ideal, but when I tried limiting sigma to
    # positive values, ROOT had trouble fitting some histograms for unknown
    # reasons.

    print('range for secondary fit(%.0f,%.0f)'%(eng*(f.GetParameter(0) -0.8*abs(f.GetParameter(1))), eng*(f.GetParameter(0) + 0.8*abs(f.GetParameter(1)))))
#    f.SetLineColor(ROOT.kBlue)
    f.SetLineColor(ROOT.kGreen+1)
    f.SetParLimits(0, 0, 2000/eng)
    f.SetParLimits(1, 0, 500/eng)
    f.SetParLimits(2, 0, 2000000)
    r = h.Fit(f, 'QLSB+', '', eng*(f.GetParameter(0) - 0.8*abs(f.GetParameter(1))), eng*(f.GetParameter(0) + 0.8*abs(f.GetParameter(1))))
    f.Write()
    h.Write()
    print('fit result: ', f.GetParameter(1))
    try:
        if not r.Get().IsValid():
            return None
    except Exception as e:
        return None
    # Use commented return statement for linear background model because it
    # adds 2 more parameters. 
    # return [f.GetParameter(i) for i in range(5)], [f.GetParError(i) for i in range(5)]
    return [f.GetParameter(i) for i in range(3)], [f.GetParError(i) for i in range(3)]
'''

def fit_gamma(h, eng, offset=0, offset_sigma=10):
    """
    Finds the full energy gamma peak (with energy `eng`) in the ROOT histogram
    `h`, and fits it with a Gausssian. Returns the fit parameters, with the
    first parameter being pC per keV.
    """
    
    ## Use TSpectrum
    nPeaks = 3

    h.GetXaxis().SetRangeUser(offset+3*offset_sigma,h.GetBinCenter(h.GetNbinsX()-1))
    _ ,peak = ROOT_peaks(h,width=15,height=0.3,npeaks=nPeaks,options='nobackground',sort=False)
    if (peak == None): peak = 100
    
    # Now we find the full energy peak. We don't use `GetMaximumBin` because we
    # want our estimate of the full energy peak to be sufficiently far away
    # from the offset peak.
    # peak_bin = None
    # for i in range(max(1, h.FindBin(offset + 3*offset_sigma)), h.GetNbinsX()):
    #     if peak_bin is None or h.GetBinContent(i) > h.GetBinContent(peak_bin):
    #         peak_bin = i
    # if peak_bin is None:
    #     return None
    # peak = h.GetBinCenter(peak_bin)

    ## Use GetMaximumBin
    #peak_bin = h.GetMaximumBin()
    #peak = h.GetBinCenter(peak_bin)
    
    # Use moving average
    # val_max = -999.
    # peak_bin = -1
    # for bin in range(int(h.GetNbinsX()/3.),h.GetNbinsX()+1-5):
    #     val_temp = 0.
    #     for it in range(5):
    #         val_temp += h.GetBinContent(bin+it)
    #     if val_temp > val_max:
    #         val_max = val_temp
    #         peak_bin = bin+2
    # peak = h.GetBinCenter(peak_bin)
    
    
    # Gaussian + linear background
    print('peak for fit: ', peak, ' -- offset = ', offset, ' -- eng=', eng)
    print('range for primary fit(%.0f,%.0f)'%(0.8*peak,1.2*peak))
    f = ROOT.TF1(f"{h.GetName()}_fit",f"[2]*exp(-0.5*(x-{offset}-[0]*{eng})**2/([1]*{eng})**2)", peak*0.5, peak*1.5)
    sigma = h.GetRMS()
    f.SetNpx(10000)
    f.SetLineColor(ROOT.kRed)
    f.SetParameter(0, (peak-offset)/eng)
    f.SetParameter(1, 0.08*peak/eng)
    f.SetParameter(2, h.GetBinContent(h.FindBin(peak)))
    r = h.Fit(f, 'QLSB+',  '', 0.8*peak, 1.2*peak)
    #h.Write()
    
    # Secondary fit that limits the range to +/-1.5 sigma. This makes the model
    # more accurate. The expression includes `abs` because sometimes ROOT makes
    # sigma negative. This is not ideal, but when I tried limiting sigma to
    # positive values, ROOT had trouble fitting some histograms for unknown
    # reasons.
    
    f.SetLineColor(ROOT.kGreen)
    #f.SetParLimits(0, 0, 2000/eng)
    #f.SetParLimits(1, 0, 500/eng)
    #f.SetParLimits(2, 0, 2000000)
    r = h.Fit(f, 'QLSB+', '', offset+eng*f.GetParameter(0) - 0.75*eng*abs(f.GetParameter(1)), offset+eng*f.GetParameter(0) + 1.*eng*abs(f.GetParameter(1)))
    f.Write()
    
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
    sigma = h.GetRMS()
    # FIXME: Gaussian noise model is just a guess. However, we really only need
    # the average noise, which a narrow Gaussian can roughly estimate.
    f = ROOT.TF1(f"{h.GetName()}_offset_fit", "gaus", -100, 100)
    f.SetNpx(10000)
    f.SetLineColor(ROOT.kRed)
    f.SetParameter(1, peak)
    f.SetParameter(2, sigma)
    f.SetParameter(0, h.GetBinContent(h.FindBin(peak)))
    r = h.Fit(f, 'Q0RLSB', '', peak-sigma, peak+sigma)
    #h.Write()
    
    # Secondary fit that we limit to +/-1 sigma. 
    r = h.Fit(f, 'QRLSB+', '', f.GetParameter(1) - abs(f.GetParameter(2)), f.GetParameter(1) + abs(f.GetParameter(2)))
    f.Write()
    #h.Write()
    
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
def fit_offset(h):
    """
    Fits the peak corresponding to 0 keV events to measure any offset.
    
    Returns the fit parameters.
    """
    peak = h.GetBinCenter(h.GetMaximumBin())
    sigma = h.GetRMS()
    # FIXME: Gaussian noise model is just a guess. However, we really only need
    # the average noise, which a narrow Gaussian can roughly estimate.
    f = ROOT.TF1(f"{h.GetName()}_offset_fit", "gaus", -100, 100)
    f.SetNpx(10000)
    f.SetLineColor(ROOT.kRed)
    f.SetParameter(1, peak)
    f.SetParameter(2, sigma)
    f.SetParameter(0, h.GetBinContent(h.FindBin(peak)))
    r = h.Fit(f, 'Q0RLSB', '', peak-sigma, peak+sigma)
    #h.Write()
    
    # Secondary fit that we limit to +/-1 sigma. 
    r = h.Fit(f, 'QRLSB+', '', f.GetParameter(1) - abs(f.GetParameter(2)), f.GetParameter(1) + abs(f.GetParameter(2)))
    f.Write()
    #h.Write()
    
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

