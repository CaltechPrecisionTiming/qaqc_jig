import numpy as np

def get_voltage_ratio(r1,r2,r3,r4):
    rp = r2 + r3*r4/(r3+r4)
    rt = (r1*rp)/(r1+rp)
    return r3*r4/(rp*(r3+r4))

def get_total_current(V,r1,r2,r3,r4):
    rp = r2 + r3*r4/(r3+r4)
    rt = (r1*rp)/(r1+rp)
    return V/rt

# Resistances for pi-pad attenuator
#
#      R2
#  ----vvvvv-------------
#    v       v          v
# R1 v       v R3    R4 v
#    v       v          v
#
R1 = 167.09
R2 = 186.49
R3 = 167.09
R4 = 50.0

# voltage output from the op amp
V = 4

ratio = get_voltage_ratio(R1,R2,R3,R4)
current = get_total_current(V,R1,R2,R3,R4)
print("voltage from op amp = %.2f V" % V)
print("ratio = %.2f" % (1/ratio))
print("output voltage = %.2f V" % (V*ratio))
print("current from op amp = %.2f mA" % (current*1000))
