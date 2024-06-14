"""quick_draw.py

TODO: Add option to set all hists per canvas to the same y-range
"""

import sys
import ROOT as rt

################################
##        USER OPTIONS        ##
################################
FN_OUT_PRE = True # default: True
SAME_YRANGE = True # default: True

TOP_MARGIN = 0.01 # default: 0.01
BOT_MARGIN = 0.01 # default: 0.01

ROOT_BATCH = True # default: True
ROOT_OPT_STAT = 0 # default: 0
#------------------------------#
#          ROOT SETUP          #
#------------------------------#
gc = []
rt.gStyle.SetOptStat(ROOT_OPT_STAT) 
rt.gROOT.SetBatch(rt.kTRUE if ROOT_BATCH else rt.kFALSE)
#------------------------------#
#   INPUT/OUTPUT FILES SETUP   #
#------------------------------#
FN_ROOT_IN = sys.argv[1]
FN_OUT_PRE = FN_ROOT_IN[:-5] + "_" if FN_OUT_PRE else ""
SOURCE = [s for s in ("lyso", "sodium", "cesium", "cobalt") if s in FN_ROOT_IN][0]
################################

def hists_per_bar(key, chA, chB=None, legend=True, set_yrange=True):
    chB = chB if chB is not None else chA+16
    if True: #"spe" in key:
        hA, hB = froot_in.Get(key+str(chA)), froot_in.Get(key+str(chB))
    # else:
    #     hA, hB = froot_in.Get(key+str(chA)+"_all"), froot_in.Get(key+str(chB)+"_all")
    #hm = hl.Clone()
    #hm.Add(hh)
    #hm.Scale(1/2)

    hhs = [hA, hB]#, hm]
    ccs = [rt.kAzure, rt.kBlack]#, rt.kRed]
    lls = ["Channel "+str(chA), "Channel "+str(chB)]#, "Average"]
    
    #hmin = min([hh.GetMinimum() for hh in hhs])
    #hmax = max([hh.GetMaximum() for hh in hhs])
    hmin = min([hh.GetBinContent(hh.GetMinimumBin()) for hh in hhs])
    hmax = max([hh.GetBinContent(hh.GetMaximumBin()) for hh in hhs])
    
    if hmin - 5*BOT_MARGIN*(hmax-hmin) < 0:
        hmin, hmax = 0, hmax + TOP_MARGIN*hmax
    else:
        mult = (1 + BOT_MARGIN + TOP_MARGIN) * (hmax - hmin)
        hmin, hmax = hmin - BOT_MARGIN*mult, hmax + TOP_MARGIN*mult
    if isinstance(set_yrange, (list, tuple)):
        if len(set_yrange) != 2:
            raise ValueError("y_range has wrong length (len(y_range)="+str(len(y_range))+")!")
        hmin, hmax, set_yrange = set_yrange, True

    for hh, cc, ll in zip(hhs, ccs, lls):
        if set_yrange:
            hh.SetMinimum(hmin)
            hh.SetMaximum(hmax)

        hh.SetLineColor(cc)
        hh.SetLineWidth(3)
        hh.SetName(ll)
        
        gc.append(hh)

    return hhs

################################

froot_in = rt.TFile(FN_ROOT_IN)
keys = [k.GetName() for k in froot_in.GetListOfKeys()]

for mode in ["spe", "source"]:
    c = rt.TCanvas("c_"+mode,"c_"+mode,4*800,4*800)
    c.Divide(4,4)

    key = (SOURCE if mode == "source" else mode) + "_ch" #+ ("_all" if mode == "source" else "") 
    hhs = [h for ch in range(16) for h in hists_per_bar(key, ch, set_yrange=(not SAME_YRANGE))]
    
    if SAME_YRANGE:
        #hmin = min([hh.GetMinimum() for hh in hhs])
        #hmax = max([hh.GetMaximum() for hh in hhs])
        hmin = min([hh.GetBinContent(hh.GetMinimumBin()) for hh in hhs])
        hmax = max([hh.GetBinContent(hh.GetMaximumBin()) for hh in hhs])
        if hmin - 5*BOT_MARGIN*(hmax-hmin) < 0:
            hmin, hmax = 0, hmax + TOP_MARGIN*hmax
        else:
            mult = (1 + BOT_MARGIN + TOP_MARGIN) * (hmax - hmin)
            hmin, hmax = hmin - BOT_MARGIN*mult, hmax + TOP_MARGIN*mult
        for hh in hhs:
            hh.SetMinimum(hmin)
            hh.SetMaximum(hmax)

    for ih, hh in enumerate(hhs):
        bar = int(ih//(len(hhs)/16))
        c.cd(bar + 1).SetGrid()
        c.cd(bar + 1).SetRightMargin(0.00)#0.04)
        hh.SetTitle(hh.GetTitle().split("for")[0] + "for bar " + str(bar))
        hh.Draw("hist" + ("same" if ih else ""))

    c.Draw()
    c.Print(FN_OUT_PRE+mode+"_charge_hists.png")

