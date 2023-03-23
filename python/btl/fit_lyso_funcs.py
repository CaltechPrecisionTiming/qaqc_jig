"""
Python module to fit the intrinsic LYSO radiation to a sum of beta decay
distributions. You can also run this module as a script to plot the LYSO charge
distribution:

    $ python fit_lyso_spectrum.py

Author: Anthony LaTorre
Date: March 21, 2023
"""
from __future__ import division
import numpy as np
from scipy.stats import norm
import ROOT
from functools import lru_cache
from scipy.special import erf

# Single photoelectron charge in attenuated mode
SPE_CHARGE = 1.0 # pC
# Single photoelectron charge standard deviation in attenuated mode
# FIXME: Should actually measure this
SPE_ERROR = 0.01 # pC

def dn(E,Q,Z,A,forb=None):
    """
    This function, dn(E,Q,Z,forb) Calculates the branch antineutrino kinetic
    energy spectrum. It takes the following variables: Q, the branch endpoint;
    Z, the number of protons in the daughter nucleus; forb, the forbiddenness.
    E the energy of the neutrino.

    From https://github.com/gzangakis/beta-spectrum/blob/master/BetaDecay.py
    """
    # FIXME: I got the beta decay distribution code from
    # https://github.com/gzangakis/beta-spectrum/blob/master/BetaDecay.py, but
    # looking at the distribution it doesn't seem to match the distribution
    # from the Nature paper at
    # https://www.nature.com/articles/s41598-018-35684-x. I don't *think* this
    # should be a big deal since our primary handle for the light yield comes
    # from the bump at around 300 keV, but should double check this.
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

# Global dictionary used to speed up the calculation when the ROOT TF1 is
# called multiple times with the same parameter
CACHE = {}

# Small number to avoid divide by zeros
EPSILON = 1e-10

@lru_cache(maxsize=None)
def p_e(es, p):
    Z = 72
    Q = 593
    A = 176 #Not sure if this is right?

    spectrum_88 = np.array([dn(e-88, Q, Z, A) for e in es],dtype=float)
    spectrum_202 = norm.pdf(es,202,10)
    spectrum_290 = np.array([dn(e-290, Q, Z, A) for e in es],dtype=float)
    spectrum_307 = norm.pdf(es,307,10)
    spectrum_395 = np.array([dn(e-395, Q, Z, A) for e in es],dtype=float)
    spectrum_509 = norm.pdf(es,509,10)
    spectrum_597 = np.array([dn(e-597, Q, Z, A) for e in es],dtype=float)

    # Add small number here in case someone is evaluating things at low
    # energies where the higher energy distributions don't have any non-zero
    # values.
    spectrum_88 += EPSILON
    spectrum_202 += EPSILON
    spectrum_290 += EPSILON
    spectrum_307 += EPSILON
    spectrum_395 += EPSILON
    spectrum_509 += EPSILON
    spectrum_597 += EPSILON

    spectrum_88 /= np.trapz(spectrum_88,x=es)
    spectrum_202 /= np.trapz(spectrum_202,x=es)
    spectrum_290 /= np.trapz(spectrum_290,x=es)
    spectrum_307 /= np.trapz(spectrum_307,x=es)
    spectrum_395 /= np.trapz(spectrum_395,x=es)
    spectrum_509 /= np.trapz(spectrum_509,x=es)
    spectrum_597 /= np.trapz(spectrum_597,x=es)

    total_spectrum = p[0]*spectrum_88 + p[1]*spectrum_202 + p[2]*spectrum_290 + p[3]*spectrum_307 + p[4]*spectrum_395 + p[5]*spectrum_509 + p[6]*spectrum_597

    return total_spectrum

def fast_norm(x,mu,sigma):
    """
    Faster version of the gaussian distribution than scipy.stats.norm which
    does a lot of checks to make sure things are in bounds.
    """
    return np.exp(-(x-mu)**2/(2*sigma**2))/(np.sqrt(2*np.pi)*sigma)

def p_q(q, y, e):
    """
    Probability of observing charge `q` assuming a light yield (pC/keV) `y` and
    energy `e` (keV).

    Note: Here we make the approximation that the spread in the number of PE
    due to the Poisson distribution dominates the SPE charge spread. In
    principle we could easily add the SPE spread, but then we wouldn't be able
    to have the analytical form in likelihood_fast().
    """
    n = y*e/SPE_CHARGE
    return fast_norm(q,y*e,np.sqrt(n)*SPE_CHARGE)

def likelihood(q,avg_y,dy,p):
    """
    Returns P(q|avg_y,dy,p) where avg_y is the average light yield, dy is the
    fractional difference between the light yield at the center and end of the
    bar, and p is a tuple containing the coefficients for the different gamma
    captures.

    A simple derivation of the likelihood is:

        p(q) = int_e p(q|e) p(e)
        p(q) = int_e int_y p(q|e,y) p(y) p(e)
        p(q) = int_e int_y p(q|e,y) p(e) # Assume p(y) = constant
        p(q) = int_e int_y Gauss(q,e*y,sqrt(e*y/SPE_CHARGE)*SPE_CHARGE) p(e)

    See the document "Fitting LYSO Intrinsic Spectrum" for more details.
    """

    ys = avg_y*np.linspace(1-dy,1+dy,10)[:,np.newaxis]
    es = np.linspace(1,1000,1000)

    # Here, we assume p(y) = constant
    return np.trapz(p_e(tuple(es),tuple(p))*np.trapz(p_q(q,ys,es)/(2*dy),x=ys,axis=0),x=es,axis=0)

def likelihood_fast(q,avg_y,dy,p):
    """
    Returns P(q|avg_y,dy,p) just like the function above, but is much faster
    since we do the second integral analytically.
    """
    es = np.linspace(1,1000,1000)

    integral = -erf((q+avg_y*(1-dy)*es)/np.sqrt(2*avg_y*(1-dy)*es*SPE_CHARGE)) \
               +erf((q+avg_y*(1+dy)*es)/np.sqrt(2*avg_y*(1+dy)*es*SPE_CHARGE))
    integral *= np.exp(q/SPE_CHARGE/2)
    integral *= np.exp(q/SPE_CHARGE/2)
    integral *= np.exp(q/SPE_CHARGE/2)
    integral *= np.exp(q/SPE_CHARGE/2)
    integral -= erf((-q+avg_y*(1-dy)*es)/np.sqrt(2*avg_y*(1-dy)*es*SPE_CHARGE))
    integral += erf((-q+avg_y*(1+dy)*es)/np.sqrt(2*avg_y*(1+dy)*es*SPE_CHARGE))
    integral *= 1/(2*es)
    return np.trapz(p_e(tuple(es),tuple(p))*integral/(2*dy),x=es,axis=0)

def lyso_spectrum(x,p):
    """
    ROOT function to return the LYSO spectrum at x[0] in keV.

    p[0] - Average Light yield (pC/keV)
    p[1] - Fractional difference between light yield at center and side
    p[2] - Constant for 88 keV spectrum
    p[3] - Constant for 202 keV gamma
    p[4] - Constant for 290 keV spectrum
    p[5] - Constant for 307 keV gamma
    p[6] - Constant for 395 keV spectrum
    p[7] - Constant for 509 keV gamma
    p[8] - Constant for 597 keV spectrum
    """
    qs = np.linspace(0,1000,1000)

    key = tuple(p[i] for i in range(9))
    if key in CACHE:
        total_spectrum = CACHE[key]
        return np.interp(x[0],qs,total_spectrum)

    ps = [p[i] for i in range(2,9)]

    total_spectrum = [likelihood(q,p[0],p[1],ps) for q in qs]

    CACHE[key] = total_spectrum

    return np.interp(x[0],qs,total_spectrum)

def get_lyso(x, p):
    f = ROOT.TF1("flyso",lyso_spectrum,0,1000,9)
    for i in range(9):
        f.SetParameter(i,p[i])
    return np.array([f.Eval(e) for e in x])

def fit_lyso(h):
    """
    Fit the internal LYSO radiation spectrum to the histogram `h`. LYSO has
    intrinsic radiation from the beta decay of 176Lu (see
    https://www.nature.com/articles/s41598-018-35684-x). Here we fit the
    histogram to a sum of beta decay spectrums offset by 88, 202+88, 307+88,
    etc. where the offsets come from the gammas emitted when the daughter
    nucleus 176Hf relaxes.

    If the fit is successful, returns a list of the fit parameters:
        p[0] - Average light yield (pC/keV)
        p[1] - Fractional difference between light yield at center and side
        p[2] - Constant for 88 keV spectrum
        p[3] - Constant for 202 keV gamma
        p[4] - Constant for 290 keV spectrum
        p[5] - Constant for 307 keV gamma
        p[6] - Constant for 395 keV spectrum
        p[7] - Constant for 509 keV spectrum
        p[8] - Constant for 597 keV spectrum

    Otherwise, returns None.
    """
    f = ROOT.TF1("flyso",lyso_spectrum,0,1000,9)
    xmax = None
    ymax = 0
    for i in range(1,h.GetNbinsX()-1):
        x = h.GetBinCenter(i)
        value = h.GetBinContent(i)
        if x > 100 and value > ymax:
            xmax = x
            ymax = value

    if xmax is None:
        return None

    # Assume peak is somewhere around 300 keV
    f.SetParameter(0,xmax/300)
    f.SetParLimits(0,0.1,10)
    f.SetParameter(1,0.1)
    f.SetParLimits(1,0.01,0.2)
    f.SetParameter(2,h.GetEntries())
    f.SetParLimits(2,0,1e9)
    f.SetParameter(3,h.GetEntries())
    f.SetParLimits(3,0,1e9)
    f.SetParameter(4,h.GetEntries())
    f.SetParLimits(4,0,1e9)
    f.SetParameter(5,h.GetEntries())
    f.SetParLimits(5,0,1e9)
    f.SetParameter(6,0)
    f.SetParLimits(6,0,1e9)
    f.SetParameter(7,0)
    f.SetParLimits(7,0,1e9)
    f.SetParameter(8,0)
    f.SetParLimits(8,0,1e9)

    # Right now we don't fit for these higher energy components. In the future
    # if we decrease the negative voltage rail we might be able to see these
    # without the waveform getting saturated at the negative rail.
    f.FixParameter(6,0)
    f.FixParameter(7,0)
    f.FixParameter(8,0)

    # Run the first fit only floating the normalization constants
    f.FixParameter(0,xmax/300)
    f.FixParameter(1,0.1)
    fr = h.Fit(f,"S+","",xmax-100,xmax+100)

    # Now we float all the parameters
    f.ReleaseParameter(0)
    f.ReleaseParameter(1)
    f.SetParLimits(0,0.1,10)
    f.SetParLimits(1,0.01,0.2)
    fr = h.Fit(f,"S+","",xmax-100,xmax+100)
    if not fr.Get().IsValid():
        return None
    return [f.GetParameter(i) for i in range(9)], [f.GetParError(i) for i in range(9)]

if __name__ == '__main__':
    import matplotlib.pyplot as plt

    x = np.linspace(0,800,800)
    f = ROOT.TF1("flyso",lyso_spectrum,0,1000,6)
    f.SetParameter(0,0.8)
    f.SetParameter(1,0.001)
    f.SetParameter(2,1)
    f.SetParameter(3,1)
    f.SetParameter(4,1)
    f.SetParameter(5,1)
    f.SetParameter(6,1)
    f.SetParameter(7,1)
    f.SetParameter(8,1)
    y = [f.Eval(e) for e in x]
    f.SetParameter(1,0.3)
    y2 = [f.Eval(e) for e in x]

    plt.plot(x,y,label='dy=0.001')
    plt.plot(x,y2,label='dy=0.2')
    plt.legend()
    plt.show()
