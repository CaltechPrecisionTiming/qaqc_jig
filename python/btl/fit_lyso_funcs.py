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
from functools import lru_cache, wraps
from scipy.special import erf
import hashlib

# Single photoelectron charge in attenuated mode (nominal SPE charge/4)
# Here we just use an approximate value instead of trying to get the actual
# fitted value for each channel since it's not very critical. The only place
# this is used is in estimating the width of the charge distribution for a
# given number of average PE due to Poisson fluctuations.
SPE_CHARGE = 1.0 # pC
# Single photoelectron charge standard deviation in attenuated mode
# FIXME: Should actually measure this
SPE_ERROR = 0.01 # pC

ES = np.linspace(1,1000,250)

def memoize(fun):
    _cache = {}

    @wraps(fun)
    def cached_fun(*args, **kwargs):
        key = []
        for arg in args:
            if isinstance(arg,np.ndarray):
                key.append(hashlib.sha1(arg).hexdigest())
            else:
                key.append(arg)
        for name, kwarg in kwargs.items():
            if isinstance(kwarg,np.ndarray):
                key.append(name + hashlib.sha1(b).hexdigest())
            else:
                key.append(name + kwarg)
        key = tuple(key)
        if key not in _cache:
            _cache[key] = fun(*args, **kwargs)
        return _cache[key]

    return cached_fun

@lru_cache(maxsize=None)
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

@memoize
def spectrum(es, offset):
    Z = 72
    Q = 593
    A = 176 #Not sure if this is right?
    rv = np.array([dn(e-offset, Q, Z, A) for e in es],dtype=float)
    # Add small number here in case someone is evaluating things at low
    # energies where the higher energy distributions don't have any non-zero
    # values.
    rv += EPSILON
    rv /= np.trapz(rv,x=es)
    return rv

SPECTRUM_88 = spectrum(ES,88)
SPECTRUM_202 = norm.pdf(ES,202,1)
SPECTRUM_290 = spectrum(ES,290)
SPECTRUM_307 = norm.pdf(ES,307,1)
SPECTRUM_395 = spectrum(ES,395)
SPECTRUM_509 = norm.pdf(ES,509,1)
SPECTRUM_597 = spectrum(ES,597)

@lru_cache(maxsize=None)
def p_e_fast(p):
    return p[0]*SPECTRUM_88 + p[1]*SPECTRUM_202 + p[2]*SPECTRUM_290 + p[3]*SPECTRUM_307 + p[4]*SPECTRUM_395 + p[5]*SPECTRUM_509 + p[6]*SPECTRUM_597

@memoize
def p_e(es, p):
    spectrum_88 = spectrum(es,88)
    spectrum_202 = fast_norm(es,202,10)
    spectrum_290 = spectrum(es,290)
    spectrum_307 = fast_norm(es,307,10)
    spectrum_395 = spectrum(es,395)
    spectrum_509 = fast_norm(es,509,10)
    spectrum_597 = spectrum(es,597)

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
    return np.trapz(p_e(es,p)*np.trapz(p_q(q,ys,es)/(2*dy),x=ys,axis=0),x=es,axis=0)

@memoize
def integral_fast(q,avg_y,dy):
    integral = -erf((q+avg_y*(1-dy)*ES)/np.sqrt(2*avg_y*(1-dy)*ES*SPE_CHARGE)) \
               +erf((q+avg_y*(1+dy)*ES)/np.sqrt(2*avg_y*(1+dy)*ES*SPE_CHARGE))
    factor = np.exp(q/SPE_CHARGE/2)
    integral *= factor
    integral *= factor
    integral *= factor
    integral *= factor
    integral -= erf((-q+avg_y*(1-dy)*ES)/np.sqrt(2*avg_y*(1-dy)*ES*SPE_CHARGE))
    integral += erf((-q+avg_y*(1+dy)*ES)/np.sqrt(2*avg_y*(1+dy)*ES*SPE_CHARGE))
    integral *= 1/(2*ES)
    return integral

def likelihood_fast(q,avg_y,dy,p):
    """
    Returns P(q|avg_y,dy,p) just like the function above, but is much faster
    since we do the second integral analytically.
    """
    integral = integral_fast(q,avg_y,dy)
    return np.trapz(p_e_fast(p)*integral,dx=ES[1]-ES[0],axis=-1)/(2*dy)

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

    ps = tuple([p[i] for i in range(2,9)])

    total_spectrum = likelihood_fast(qs[:,np.newaxis],p[0],p[1],ps)

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
    f = ROOT.TF1("%s_fit" % h.GetName(),lyso_spectrum,0,1000,9)
    xmax = None
    ymax = 0
    xmin = None
    ymin = 0
    for i in range(1,h.GetNbinsX()-1):
        x = h.GetBinCenter(i)

        if x > h.GetStdDev()*2:
            break

        value = h.GetBinContent(i)

        if xmin is None or value < ymin:
            xmin = x
            ymin = value

    for i in range(1,h.GetNbinsX()-1):
        x = h.GetBinCenter(i)
        value = h.GetBinContent(i)
        if x > xmin and (xmax is None or value > ymax):
            xmax = x
            ymax = value

    if xmax is None:
        return None

    dx = h.GetBinCenter(2) - h.GetBinCenter(1)

    # Assume peak is somewhere around 300 keV
    pc_per_kev = xmax/300

    f.SetParameter(0,pc_per_kev)
    f.SetParLimits(0,0.1,10)
    f.SetParameter(1,0.1)
    f.SetParLimits(1,0.01,0.2)
    f.SetParameter(2,0.25*h.GetEntries()/dx)
    f.SetParLimits(2,0,1e9)
    f.SetParameter(3,0)
    f.SetParLimits(3,0,1e9)
    f.SetParameter(4,0.25*h.GetEntries()/dx)
    f.SetParLimits(4,0,1e9)
    f.SetParameter(5,0)
    f.SetParLimits(5,0,1e9)
    f.SetParameter(6,0.25*h.GetEntries()/dx)
    f.SetParLimits(6,0,1e9)
    f.SetParameter(7,0)
    f.SetParLimits(7,0,1e9)
    f.SetParameter(8,0.25*h.GetEntries()/dx)
    f.SetParLimits(8,0,1e9)

    # Right now we don't fit for these higher energy components. In the future
    # if we decrease the negative voltage rail we might be able to see these
    # without the waveform getting saturated at the negative rail.
    #f.FixParameter(6,0)
    #f.FixParameter(7,0)
    #f.FixParameter(8,0)

    # Run the first fit only floating the normalization constants
    f.FixParameter(0,xmax/300)
    f.FixParameter(1,0.1)
    fr = h.Fit(f,"S","",pc_per_kev*50,800)
    h.GetXaxis().SetRangeUser(xmin,800)
    h.Write()

    # Now we float all the parameters
    f.ReleaseParameter(0)
    f.ReleaseParameter(1)
    f.SetParLimits(0,0.1,10)
    f.SetParLimits(1,0.01,0.2)
    fr = h.Fit(f,"S","",pc_per_kev*50,800)
    if not fr.Get().IsValid():
        return None
    h.GetXaxis().SetRangeUser(xmin,800)
    f.Write()
    h.Write()
    return [f.GetParameter(i) for i in range(9)], [f.GetParError(i) for i in range(9)]

if __name__ == '__main__':
    import matplotlib.pyplot as plt

    x = np.linspace(0,800,800)
    f = ROOT.TF1("flyso",lyso_spectrum,0,1000,9)
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
