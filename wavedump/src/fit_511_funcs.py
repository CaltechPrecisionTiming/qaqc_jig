from __future__ import print_function, division
import numpy as np
import sys
import ROOT

def peaks(h, width=4, height=0.05, options=""):
    """
    Finds peaks in hisogram `h`. `height` is measured as a fraction of the
    highest peak.  Peaks lower than `<highest peak>*height` will not be
    searched for.

    Returns an array of the peak locations, sorted in charge order, lowest to
    highest `x_pos`, and the location of the highest peak:
    (`x_pos`, `highest_peak`)
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

def fit_511(h, bias):
    """
    511 Peak Finding Strategy
    
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
    
    Older than June 2022:
         Peak searching using the TSpectrum class. See ROOT
         documentation for details.
         https://root.cern.ch/root/htmldoc/guides/spectrum/Spectrum.html#processing-and-visualization-functions

         *** depending on the overvoltage the peak dimension with
         respect to the noise makes changes needed on the second
         value. A more efficient way would be to create a bisection
         and stop it when it detects only the 2 biggest peaks.
         The number of found peaks and their positions are written into
         the members fNpeaks and fPositionX 
         Search(const TH1 * hin, Double_t sigma = 2, Option_t * option = "", Double_t threshold = 0.05)
         Peaks with amplitude < th*Amax are discarded. If too small ->
         detects peaks after the 511 peak. The code right now detects
         the last peak (that doesn't have a huge width) as the 511
         peak. Other option is to iterate through the y positions and
         keep always the latest peak with an increase over the
         previous ones? first derivative with respect to max/min
         closest to zero       
    """
    x_pos = peaks(h, width=2, height=0.05, options="nobackground")[0]
    
    print("Peak charge full list:")
    print(x_pos)
    
    f1 = None
    win = 0.2 * h.GetStdDev()
    for i in range(len(x_pos)-1, -1, -1):
        peak = x_pos[i]
        if peak < x_pos[0] + 0.01*h.GetStdDev():
            # All the remaining peaks will be closer to `x_pos[0]`.
            break
        print(f'Fitting this peak! {peak}')
        f1 = ROOT.TF1("f1","gaus", peak-win, peak+win)
        r = h.Fit(f1, 'ILMSR+')
        r = r.Get()
        if f1.GetParameter(1) < x_pos[0]:
            # Impossible that this is the 511 peak 
            f1 = None
            continue
        if not r.IsValid() or f1.GetParameter(2) > 100:
            # Not impossible that this is the 511 peak, but there might be a
            # better peak later in the loop
            continue
        else:
            # Found the 511 peak!
            break
    h.Write()
    if f1 == None:
        return None
    else:
        return (f1.GetParameter(1), f1.GetParError(1))
