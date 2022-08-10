from __future__ import print_function, division
import csv
import os
import ROOT
from ROOT import gROOT
from ROOT import TMath
import matplotlib.pyplot as plt

canvas = []
canvas_count = 1

def save_as_csv(data_type, value, value_err, channel, output):
    """
    Saves charge data in `output` (csv format). `output` can be read by
    `save_charge_data.py`, which organizes the data and saves it to a more
    permanent csv file.
    """
    with open (output, 'a') as file:
        headers = [
            'Data Type',
            'Channel',
            'Charge',
            'Err Abs'
            ]
        writer = csv.DictWriter(file, headers)
        if os.stat('temp_data.csv').st_size == 0:
            writer.writeheader()
        writer.writerow({
            'Data Type': data_type,
            'Channel': channel,
            'Charge': value,
            'Err Abs': value_err
        })

def read_hists(f, spe=False):
    # raw is unfiltered
    raw_histograms = {}
    filtered_histograms = {}
    key_titles = []
    for key in f.GetListOfKeys():
        if key.GetTitle() in key_titles:
            continue
        key_titles.append(key.GetTitle())
        
        h = key.ReadObj()

        if h.ClassName() != 'TH1F' and h.ClassName() != 'TH1D':
            continue

        if h.GetName().startswith('f_'):
            filtered_histograms[h.GetName()] = h
        else:
            raw_histograms[h.GetName()] = h
    if spe:
        return (raw_histograms, filtered_histograms)
    else:
        return raw_histograms.values()

def plot(h):
    global canvas
    global canvas_count
    canvas.append(ROOT.TCanvas("c%i" % canvas_count))
    canvas_count += 1
    h.Draw()
    canvas[-1].Update()

def print_pdfs(h):
    global canvas
    global canvas_count
    root, ext = os.path.splitext(filename)
    canvas[-1].Print(os.path.join(args.print_pdfs, f"{root}_{h.GetName()}.pdf"))

if __name__ == '__main__':
    from argparse import ArgumentParser
    from fit_511_funcs import *
    from fit_spe_funcs import *

    parser = ArgumentParser(description='Fit SPE and sodium charge histograms')
    parser.add_argument('--file-511', required=True, help='511 charge histogram filename (ROOT format)')
    parser.add_argument('--file-spe', required=True, help='spe charge histogram filename (ROOT format)')
    parser.add_argument('--plot', default=False, action='store_true', help='plot the waveforms and charge integral')
    parser.add_argument('--bias', type=float,default=42, help='Bias Voltage input')
    parser.add_argument('--root-func', default=False, action='store_true', help='Use the custom root function (poisson model) to preform the fit. Otherwise, use the custom python function (Vinogradov model).')
    parser.add_argument('--chi2', default=False, action='store_true', help='Save the ndof and chi2 value of the SPE fit to csv')
    parser.add_argument("--print-pdfs", default=None, type=str, help="Folder to save pdfs in")
    # parser.add_argument("-o", "--output", default=None, type=str, help="File to write charge data to")

    args = parser.parse_args()
    
    if not args.plot:
        # Disables the canvas from ever popping up
        gROOT.SetBatch()
    
    ################
    # 511 FITTING
    ################
    charge_511 = {}
    f_511 = ROOT.TFile(args.file_511, "UPDATE")
    for h in read_hists(f_511, spe=False):
        fit_output = fit_511(h, args.bias)
        if not fit_output:
            # What should we do if the fit is unsuccessful?
            print('511 fit unsuccessful!')
        else:
            charge_511[h.GetName()] = fit_output

        if args.plot or args.print_pdfs:
            plot(h)
        if args.print_pdfs:
            print_pdfs(h)
    ################
    # SPE FITTING
    ################
    charge_spe = {}
    f_spe = ROOT.TFile(args.file_spe, "UPDATE")
    raw_histograms, filtered_histograms = read_hists(f_spe, spe=True)
    for h in raw_histograms.values():
        charge_spe[h.GetName()] = fit_spe(h, filtered_histograms, root_func=args.root_func)            

        if args.plot or args.print_pdfs:
            plot(h)
        if args.print_pdfs:
            print_pdfs(h) 
    
    # FIXME: Upload this information to the website
    print('Light output:')
    for ch in charge_511:
        if not ch in charge_spe:
            print(f'{ch} does not have any SPE data!')
        else:
            print(f'{ch}: {charge_511[ch]/charge_spe[ch]/0.511}')
    for ch in charge_spe:
        if not ch in charge_511:
            print('f{ch} does not have any 511 data!')

    if args.plot:
        input()
    f_511.Close()
    f_spe.Close()
