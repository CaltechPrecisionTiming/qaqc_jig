from __future__ import print_function, division
import csv
import os
import sys
import ROOT
from ROOT import gROOT
from ROOT import TMath
import matplotlib.pyplot as plt

canvas = []

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

def make_plots_pdfs(h, pdfs=False, filename=None):
    global canvas
    # Naming canvases this way will produce a runtime warning because ROOT
    # will always make a default canvas with name `c1` the first time you
    # fit a histogram. The only way I know how to get rid of it is to
    # overwrite it like this.
    c = ROOT.TCanvas(f'c{len(canvas)+1}')
    canvas.append(c)
    h.Draw()
    c.Update()
    if pdfs:
        if not filename:
            print('No filename specified; can not print pdf!')
        else:
            root, ext = os.path.splitext(filename)
            c.Print(os.path.join(args.print_pdfs, f"{root}_{h.GetName()}.pdf"))

if __name__ == '__main__':
    from argparse import ArgumentParser
    from fit_511_funcs import *
    from fit_spe_funcs import *

    parser = ArgumentParser(description='Fit SPE and sodium charge histograms')
    parser.add_argument('--file-511', required=True, help='511 charge histogram filename (ROOT format)')
    parser.add_argument('--file-spe', required=True, help='spe charge histogram filename (ROOT format)')
    parser.add_argument('--plot', default=False, action='store_true', help='plot the waveforms and charge integral')
    parser.add_argument('--root-func', default=False, action='store_true', help='Use the custom root function (poisson model) to preform the fit. Otherwise, use the custom python function (Vinogradov model).')
    parser.add_argument('--chi2', default=False, action='store_true', help='Save the ndof and chi2 value of the SPE fit to csv')
    parser.add_argument("--print-pdfs", default=None, type=str, help="Folder to save pdfs in")
    parser.add_argument("--test-511", default=False, action='store_true', help="Only perform the 511 fitting (only for testing purposes)")
    parser.add_argument("--test-spe", default=False, action='store_true', help="Only perform the SPE fitting (only for testing purposes)")
    parser.add_argument("-o", "--output", default=None, type=str, help="File to write charge data to")
    
    args = parser.parse_args()
    
    if args.test_511 and args.test_spe:
        print('test-511 and test-spe are incompatible arguments! Quitting...', file=sys.stderr)
        sys.exit(1)
    
    if not args.plot:
        # Disables the canvas from ever popping up
        gROOT.SetBatch()
    
    opened = []

    ################
    # 511 FITTING
    ################
    if not args.test_spe:
        charge_511 = {}
        f_511 = ROOT.TFile(args.file_511, "UPDATE")
        opened.append(f_511)
        for h in read_hists(f_511, spe=False):
            print(h.GetName())
            h.GetListOfFunctions().Clear()
            fit_output = fit_511(h)
            if not fit_output:
                # FIXME Handle this case better; we should make `fit_511` reliable
                # enough to not return null
                print('511 fit unsuccessful!')
            else:
                charge_511[h.GetName()] = fit_output
            
            if args.plot or args.print_pdfs:
                make_plots_pdfs(h, pdfs=args.print_pdfs, filename=args.file_511)
    ################
    # SPE FITTING
    ################
    if not args.test_511:
        charge_spe = {}
        f_spe = ROOT.TFile(args.file_spe, "UPDATE")
        opened.append(f_spe)
        raw_histograms, filtered_histograms = read_hists(f_spe, spe=True)
        for h in raw_histograms.values():
            print(h.GetName())
            h.GetListOfFunctions().Clear()
            if args.root_func:
                model = ROOT_FUNC
            else:
                # When using a python function, the model can't be deleted if we
                # want to plot this histogram later, which is why it must be
                # created here.
                model = vinogradov_model()
            charge_spe[h.GetName()] = fit_spe(h, filtered_histograms, model, root_func=args.root_func)            

            if args.plot or args.print_pdfs:
                make_plots_pdfs(h, pdfs=args.print_pdfs, filename=args.file_spe)
    ################
    # LIGHT OUTPUT CALCULATION
    ################
    # FIXME: Upload this information to the website
    if not (args.test_511 or args.test_spe):
        print('Light output:')
        for ch in charge_511:
            if ch in charge_spe:
                print(f'{ch}: {charge_511[ch][0]/charge_spe[ch][0]/0.511}')
            else:
                print(f'{ch} does not have any SPE data!')
        for ch in charge_spe:
            if not ch in charge_511:
                print(f'{ch} does not have any 511 data!')
    elif args.test_511:
        for ch in charge_511:
            print(f'{ch} 511 charge: {charge_511[ch][0]}')
    elif args.test_spe:
        for ch in charge_spe:
            print(f'{ch} spe charge: {charge_spe[ch][0]}')
            if args.output:
                save_as_csv('SPE', charge_spe[ch][0], charge_spe[ch][1], ch, args.output)
    
    if args.plot:
        input()
    for f in opened:
        f.Close()
