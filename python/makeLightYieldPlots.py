import numpy as np
import ROOT
from ROOT import TGraphErrors
import sys,os
from typing import Tuple
from pathlib import Path

gc = []

if len(sys.argv) != 3:
	raise RuntimeError("Usage: python3 makeLightYieldPlots.py input_dir output_dir")

inputDirectory = Path(sys.argv[1])
outputDirectory = Path(sys.argv[2])
outputDirectory.mkdir(exist_ok=True, parents=True)

numBars=16
lastChannel=31
lastLeftChannel = int(((lastChannel+1)/2)-1) #15 for 32 channels


c1 = ROOT.TCanvas() #Define Canvas for Plots
ROOT.gROOT.SetBatch() #Don't pop up canvases

def extractFitInfo(file_path: str, source: str, channel: int) -> Tuple[float, float]:
	#returns tuple with (lightYield, lightYieldError) by extracting info from fitted hists
	f = ROOT.TFile(file_path)
	# histogram = file.Get(f"{source}_ch{channel};3")
	histogram = f.Get("{}_ch{};3".format(source, channel))
	f.Close()
	
	fit_functions = histogram.GetListOfFunctions()
	if len(fit_functions) < 1:
		raise RuntimeError("No fit functions found in filepath='{}', source='{}', channel='{}'".format(file_path, source, channel))

	fit_function = fit_functions[0]
	
	# get relevant info
	spe_charge = fit_function.GetParameter(3)
	spe_charge_err = fit_function.GetParError(3)
	std_scope_noise = fit_function.GetParameter(4)
	std_scope_dev = fit_function.GetParameter(5)
	
	return (spe_charge, np.sqrt(std_scope_noise**2 + std_scope_dev**2))


def makeLightYieldPlot(file_path, source):
	#for given module/source, find light yield and associated error
	leftChannelLightYields  = []; leftChannelLightYieldsErrors = []
	rightChannelLightYields  = []; rightChannelLightYieldsErrors = []
	averageLightYields  = []; averageLightYieldsErrors = []	
	for channelNum in range(lastLeftChannel+1):
		channelInfoTuple = extractFitInfo(file_path, source, channelNum)
		leftChannelLightYields.append(channelInfoTuple[0])
		leftChannelLightYieldsErrors.append(channelInfoTuple[1])
	for channelNum in range(lastChannel, lastLeftChannel, -1):
		channelInfoTuple = extractFitInfo(file_path, source, channelNum)
		rightChannelLightYields.append(channelInfoTuple[0])
		rightChannelLightYieldsErrors.append(channelInfoTuple[1])
	for bar in range(numBars):
		averageLightYields.append((leftChannelLightYields[bar]+rightChannelLightYields[bar])/2)
		averageLightYieldsErrors.append((leftChannelLightYieldsErrors[bar]+rightChannelLightYieldsErrors[bar])/2)

	#make TGraph Objects for both sides of the bar and the average
	print(numBars, np.arange(numBars) + 1, leftChannelLightYields, np.zeros(numBars), leftChannelLightYieldsErrors)
	leftSideGraph = ROOT.TGraphErrors(numBars, np.arange(numBars, dtype=np.float) + 1, np.array(leftChannelLightYields), np.zeros(numBars, dtype=np.float), np.array(leftChannelLightYieldsErrors))
	leftSideGraph.SetLineColor(2)
	rightSideGraph = ROOT.TGraphErrors(numBars, np.arange(numBars, dtype=np.float) + 1, np.array(rightChannelLightYields), np.zeros(numBars, dtype=np.float), np.array(rightChannelLightYieldsErrors))
	rightSideGraph.SetLineColor(3)	
	totalGraph = ROOT.TGraphErrors(numBars, np.arange(numBars, dtype=np.float) + 1, np.array(averageLightYields), np.zeros(numBars, dtype=np.float), np.array(averageLightYieldsErrors))
	title = "Module " + file_path.split("/")[-1].split("_")[1]  # Module xxxx

	totalGraph.SetTitle(title)
	totalGraph.GetYaxis().SetTitle("SPE Charge (pC)")
	totalGraph.GetXaxis().SetTitle("Bar")
	
	totalGraph.SetMinimum(0)
	leftSideGraph.SetMinimum(0)
	rightSideGraph.SetMinimum(0)

	legend  = ROOT.TLegend(0.1, 0.1, 0.2, 0.2)
	legend.AddEntry(leftSideGraph, "Left", "l")
	legend.AddEntry(rightSideGraph, "Right", "l")
	legend.AddEntry(totalGraph, "Mean", "l")
	
	#Draw TGraphs on Canvas, Print Canvas to output directory
	totalGraph.Draw()
	leftSideGraph.Draw("SAME")
	rightSideGraph.Draw("SAME")
	
	legend.Draw()
	# c1.Update()

	# parse module
	module = title.replace(" ", "").lower()
	c1.Print(str(outputDirectory / str("LightYield_"+str(module)+"_"+str(source)+".png")))
	c1.Clear()

# find all *.root
root_files = list(inputDirectory.glob("*.root"))
print("number of root files to process: " + str(len(root_files)))
print("root_files=" + str(root_files))

for root_file in root_files:
	file_path = str(root_file)
	print("Making plot for " + file_path)
	source = "spe"
	makeLightYieldPlot(file_path, source)








