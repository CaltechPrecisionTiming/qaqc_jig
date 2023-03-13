from __future__ import division
import math
import numpy as np, scipy.constants as sciconst, scipy.special as ss,  matplotlib.pyplot as plt, sys,re
from scipy.integrate import simps

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

def lyso_spectrum(x,p):
    """
    ROOT function to return the LYSO spectrum at x[0] in keV.
    """
    Z = 72
    Q = 593
    A = 176 #Not sure if this is right?

    es = np.linspace(0,10000,10000)
    spectrum_80 = np.array([dn(e-80, Q, Z, A) for e in es])
    spectrum_290 = np.array([dn(e-290, Q, Z, A) for e in es])
    spectrum_395 = np.array([dn(e-395, Q, Z, A) for e in es])
    spectrum_597 = np.array([dn(e-597, Q, Z, A) for e in es])

    spectrum_80 /= np.trapz(spectrum_80,x=es)
    spectrum_290 /= np.trapz(spectrum_290,x=es)
    spectrum_395 /= np.trapz(spectrum_395,x=es)
    spectrum_597 /= np.trapz(spectrum_597,x=es)

    total_spectrum = p[0]*spectrum_80 + p[1]*spectrum_290 + p[2]*spectrum_395 + p[3]*spectrum_597

    return np.interp(x[0],es,total_spectrum)
