from __future__ import division
import numpy as np
from scipy.stats import norm
import ROOT

def dn(E,Q,Z,A,forb=None):
    """
    This function, dn(E,Q,Z,forb) Calculates the branch antineutrino kinetic
    energy spectrum. It takes the following variables: Q, the branch endpoint;
    Z, the number of protons in the daughter nucleus; forb, the forbiddenness.
    E the energy of the neutrino.

    From https://github.com/gzangakis/beta-spectrum/blob/master/BetaDecay.py
    """

    e = E

    if not 0 < E <= Q:
        return 0

    # Fermi Function approximation
    if e < 613.2:
        a = - 0.811 + (4.46e-2 * Z) + (1.08e-4 * (Z**2))
        b = 0.673 - (1.82e-2 * Z) + (6.38e-5 * (Z**2))

    if e >= 613.2:
        a = - 8.46e-2 + (2.48e-2 * Z) + (2.37e-4*(Z**2))
        b = 1.15e-2 + (3.58e-4 * Z) - (6.17e-5*(Z**2))

    P = np.sqrt(np.power(e+511,2)-np.power(511,2))
    F = ((e+511)/P) * np.exp( a + (b * np.sqrt(((e+511)/511)-1)))
    # Define Forbiddenness correction
    if forb=='1U':
        forbiddenness = P**2+(Q-e)**2
    elif forb=='2U':
        forbiddenness=(Q-e)**4+(10/3)*P**2*(Q-e)**2+P**2
    elif forb=='3U':
        forbiddenness=(Q-e)**6+7*P**2*(Q-e)**4+7*P**4*(Q-e)**2+P**6
    else: 
        forbiddenness=1

    # Branch Spectrum
    return forbiddenness*F*(np.sqrt(np.power(e,2)+2*e*511)*np.power(Q-e,2)*(e+511))

# Photons per keV
LIGHT_YIELD = 1500/1000.0

CACHE = {}

def lyso_spectrum(x,p):
    """
    ROOT function to return the LYSO spectrum at x[0] in keV.

    p[0] - Constant for 80 keV spectrum
    p[1] - Constant for 290 keV spectrum
    p[2] - Constant for 395 keV spectrum
    p[3] - Constant for 597 keV spectrum
    p[4] - Light yield (pC/keV)
    p[5] - Multiplier for light yield when calculating sigma
    """
    Z = 72
    Q = 593
    A = 176 #Not sure if this is right?

    es = np.linspace(0,1000,1000)

    key = tuple(p[i] for i in range(6))
    if key in CACHE:
        total_spectrum = CACHE[key]
        return np.interp(x[0]/p[4],es,total_spectrum)

    spectrum_80 = np.array([dn(e-80, Q, Z, A) for e in es])
    spectrum_290 = np.array([dn(e-290, Q, Z, A) for e in es])
    spectrum_395 = np.array([dn(e-395, Q, Z, A) for e in es])
    spectrum_597 = np.array([dn(e-597, Q, Z, A) for e in es])

    spectrum_80 /= np.trapz(spectrum_80,x=es)
    spectrum_290 /= np.trapz(spectrum_290,x=es)
    spectrum_395 /= np.trapz(spectrum_395,x=es)
    spectrum_597 /= np.trapz(spectrum_597,x=es)

    width_80 = np.sqrt(80*LIGHT_YIELD*p[5])*p[4]/(LIGHT_YIELD*p[5])
    gauss_x = np.arange(-100,100,es[1]-es[0])
    gauss_y = norm.pdf(gauss_x,scale=width_80)
    spectrum_80 = np.convolve(spectrum_80,gauss_y,'same')

    width_290 = np.sqrt(290*LIGHT_YIELD*p[5])*p[4]/(LIGHT_YIELD*p[5])
    gauss_x = np.arange(-100,100,es[1]-es[0])
    gauss_y = norm.pdf(gauss_x,scale=width_290)
    spectrum_290 = np.convolve(spectrum_290,gauss_y,'same')

    width_395 = np.sqrt(395*LIGHT_YIELD*p[5])*p[4]/(LIGHT_YIELD*p[5])
    gauss_x = np.arange(-100,100,es[1]-es[0])
    gauss_y = norm.pdf(gauss_x,scale=width_395)
    spectrum_395 = np.convolve(spectrum_395,gauss_y,'same')

    width_597 = np.sqrt(597*LIGHT_YIELD*p[5])*p[4]/(LIGHT_YIELD*p[5])
    gauss_x = np.arange(-100,100,es[1]-es[0])
    gauss_y = norm.pdf(gauss_x,scale=width_597)
    spectrum_597 = np.convolve(spectrum_597,gauss_y,'same')

    total_spectrum = p[0]*spectrum_80 + p[1]*spectrum_290 + p[2]*spectrum_395 + p[3]*spectrum_597

    CACHE[key] = total_spectrum

    return np.interp(x[0]/p[4],es,total_spectrum)

def get_lyso(x, p):
    f = ROOT.TF1("flyso",lyso_spectrum,0,1000,6)
    for i in range(6):
        f.SetParameter(i,p[i])
    return np.array([f.Eval(e) for e in x])

def fit_lyso(h):
    """
    Fit the internal LYSO radiation spectrum to the histogram `h`. LYSO has
    intrinsic radiation from the beta decay of 176Lu (see
    https://www.nature.com/articles/s41598-018-35684-x). Here we fit the
    histogram to a sum of beta decay spectrums offset by 88, 202+80, 307+80,
    etc. where the offsets come from the gammas emitted when the daughter
    nucleus 176Hf relaxes.

    If the fit is successful, returns a list of the fit parameters:
        p[0] - Constant for 80 keV spectrum
        p[1] - Constant for 290 keV spectrum
        p[2] - Constant for 395 keV spectrum
        p[3] - Constant for 597 keV spectrum
        p[4] - Light yield (pC/keV)
        p[5] - Multiplier for light yield when calculating sigma

    Otherwise, returns None.
    """
    f = ROOT.TF1("flyso",lyso_spectrum,0,1000,6)
    xmax = 0
    ymax = 0
    for i in range(1,h.GetNbinsX()-1):
        x = h.GetBinCenter(i)
        value = h.GetBinContent(i)
        if x > 100 and value > ymax:
            xmax = x
            ymax = value
    # Assume peak is somewhere around 300 keV
    f.SetParameter(0,h.GetEntries())
    f.SetParLimits(0,0,1e9)
    f.SetParameter(1,h.GetEntries())
    f.SetParLimits(1,0,1e9)
    f.SetParameter(2,0)
    f.SetParLimits(2,0,1e9)
    f.SetParameter(3,0)
    f.SetParLimits(3,0,1e9)
    f.SetParameter(4,xmax/300)
    f.SetParLimits(4,0.1,10)
    f.SetParameter(5,1)
    f.SetParLimits(5,0.1,10)

    # Right now we don't fit for these higher energy components. In the future
    # if we decrease the negative voltage rail we might be able to see these
    # without the waveform getting saturated at the negative rail.
    f.FixParameter(2,0)
    f.FixParameter(3,0)

    # Run the first fit only floating the normalization constants
    f.FixParameter(4,xmax/300)
    f.FixParameter(5,1)
    fr = h.Fit(f,"S+","",xmax-100,xmax+100)

    # Now we float all the parameters
    f.ReleaseParameter(4)
    f.ReleaseParameter(5)
    fr = h.Fit(f,"S+","",xmax-100,xmax+100)
    if not fr.Get().IsValid():
        return None
    return [f.GetParameter(i) for i in range(6)]

if __name__ == '__main__':
    import matplotlib.pyplot as plt

    x = np.linspace(0,1000,1000)
    f = ROOT.TF1("flyso",lyso_spectrum,0,1000,6)
    f.SetParameter(0,1)
    f.SetParameter(1,1)
    f.SetParameter(2,0)
    f.SetParameter(3,0)
    f.SetParameter(4,1)
    f.SetParameter(5,1)
    y = [f.Eval(e) for e in x]

    plt.plot(x,y)
    plt.show()
