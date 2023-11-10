import os
import numpy as np
import ROOT


def GetMaximum(g):
    maximum = -999999.
    for point in range(g.GetN()):
        if g.GetPointY(point) > maximum:
            maximum = g.GetPointY(point)
    return maximum


def iqr(x):
    return np.percentile(x,75) - np.percentile(x,25)


def get_bins(x, cutoff=None):
    """
    Returns bins for the data `x` using the Freedman Diaconis rule. See
    https://en.wikipedia.org/wiki/Freedman%E2%80%93Diaconis_rule.
    """
    x = np.asarray(x)
    
    orig_x = x.copy()
    
    if cutoff is not None:
        x = x[x > cutoff]
    
    if len(x) == 0:
        return np.arange(0,100,1)
    
    bin_width = 0.5 * iqr(x)/(len(x)**(1/3.0))
    
    if bin_width == 0:
        #print('Zero bin width! Quitting...', file=sys.stderr)
        #sys.exit(1)
        print('Zero bin width! Setting bin width to 1', file=sys.stderr)
        bin_width = 1
    
    first = np.percentile(orig_x,1)
    last = np.percentile(x,99)
    return np.arange(first,last,bin_width)


def chunks(lst, n):
    """
    Yield successive n-sized chunks from lst.
    """
    for i in range(0, len(lst), n):
        yield (i,i + n)


def plot_time_volt(x, y, channel, data_type, a, b, avg_y=None, pdf=False, filename=None):
    plt.figure()
    plt.subplot(2,1,1)
    plt.plot(x,y[:100].T)
    plt.xlabel("Time (ns)")
    plt.ylabel("Voltage (V)")
    plt.axvline(x[a])
    plt.axvline(x[b])
    plt.subplot(2,1,2)
    if avg_y is not None:
        plt.plot(x,avg_y)
    else:
        plt.plot(x, np.median(y, axis=0))
    plt.xlabel("Time (ns)")
    plt.ylabel("Voltage (V)")
    plt.axvline(x[a])
    plt.axvline(x[b])
    plt.suptitle("%s %s" % (data_type, channel))
    if args.print_pdfs:
        if not filename:
            print('No filename specified; can not print pdf!')
        else:
            root, ext = os.path.splitext(filename)
            plt.savefig(os.path.join(args.print_pdfs, "%s_%s_%s_TimeVolt.pdf" % (root,data_type,channel)))


def plot_hist(h, path=None, filename=None, logy=False):
    # Naming canvases this way will produce a runtime warning because ROOT
    # will always make a default canvas with name `c1` the first time you
    # fit a histogram. The only way I know how to get rid of it is to
    # overwrite it like this.
    c = ROOT.TCanvas('c1_%s'%h.GetName(),'',700,600)
    h.Draw()
    ROOT.gPad.SetGridx()
    if logy:
        ROOT.gPad.SetLogy()
    c.Update()
    if not filename or not path:
        print('No path / filename specified; can not print pdf!')
    else:
        root, ext = os.path.splitext(filename)
        if not os.path.isdir("%s/%s"%(path,root)):
            os.mkdir("%s/%s"%(path,root))
        
        #print('Printing plots to file: %s/%s/%s' % (args.print_pdfs,root, h.GetName() ) )
        c.Print("%s/%s/%s.pdf" % (path, root, h.GetName()))
        c.Print("%s/%s/%s.png" % (path, root, h.GetName()))


def plot_graph(g, path=None, filename=None, xMin=-1., xMax=32., yMin=-1., yMax=-1.):
    global canvas
    # Naming canvases this way will produce a runtime warning because ROOT
    # will always make a default canvas with name `c1` the first time you
    # fit a histogram. The only way I know how to get rid of it is to
    # overwrite it like this.
    c = ROOT.TCanvas('c1_%s'%g.GetName(),'',700,600)
    g.SetMarkerStyle(20)
    g.SetMarkerColor(ROOT.kBlack)
    mean = g.GetMean(2)
    rms = g.GetRMS(2)    
    hPad = None
    if yMin == yMax:
        hPad = ROOT.gPad.DrawFrame(xMin,0,xMax,max(mean*2, mean+5*rms))
    else:
        hPad = ROOT.gPad.DrawFrame(xMin,yMin,xMax,yMax)
    hPad.SetTitle(";%s;%s"%(g.GetXaxis().GetTitle(),g.GetYaxis().GetTitle()))
    hPad.Draw()
    g.Draw('PL,same')
    latex = ROOT.TLatex( 0.20, 0.90, 'mean = %.2e, RMS = %.1f%%'%(mean,rms/mean*100.))
    latex.SetNDC()
    latex.SetTextSize(0.050)
    latex.SetTextColor(ROOT.kBlack)
    latex.Draw()
    ROOT.gPad.SetGridy()
    c.Update()
    if not path or not filename:
            print('No path / filename specified; can not print pdf!')
    else:
        root, ext = os.path.splitext(filename)
        if not os.path.isdir("%s/%s"%(path,root)):
            os.mkdir("%s/%s"%(path,root))
        c.Print("%s/%s/%s.pdf" % (path, root, g.GetName()))
        c.Print("%s/%s/%s.png" % (path, root, g.GetName()))


def plot_graph_bars(g_L, g_R, g_A, path=None, filename=None, graphname='graph', yMin=-1, yMax=-1):
    # Naming canvases this way will produce a runtime warning because ROOT
    # will always make a default canvas with name `c1` the first time you
    # fit a histogram. The only way I know how to get rid of it is to
    # overwrite it like this.
    c = ROOT.TCanvas('c1_%s'%g_A.GetName(),'',700,600)
    mean_L = g_L.GetMean(2)
    mean_R = g_R.GetMean(2)
    mean_A = g_A.GetMean(2)
    mean = 0.5*(mean_L+mean_R)
    rms_L = g_L.GetRMS(2)
    rms_R = g_R.GetRMS(2)
    rms_A = g_A.GetRMS(2)
    rms = max(rms_L,rms_R)
    if yMin == yMax:
        hPad = ROOT.gPad.DrawFrame(-1.,0,16.,max(mean*2,mean+5.*rms))
    else:
        hPad = ROOT.gPad.DrawFrame(-1.,yMin,16.,yMax)        
    hPad.SetTitle(";%s;%s"%(g_L.GetXaxis().GetTitle(),g_L.GetYaxis().GetTitle()))
    hPad.Draw()
    g_L.SetMarkerStyle(20)
    g_L.SetMarkerColor(ROOT.kRed)
    g_L.SetLineColor(ROOT.kRed)
    g_R.SetMarkerStyle(20)
    g_R.SetMarkerColor(ROOT.kBlue)
    g_R.SetLineColor(ROOT.kBlue)
    g_A.SetMarkerStyle(20)
    g_A.SetMarkerColor(ROOT.kBlack)
    g_A.SetLineColor(ROOT.kBlack)
    g_L.Draw('PL,same')
    g_R.Draw('PL,same')
    g_A.Draw('PL,same') 
    latex_L = ROOT.TLatex( 0.20, 0.90, '  left side:   mean = %.2e, RMS = %.1f%%'%(mean_L,rms_L/mean_L*100.))
    latex_L.SetNDC()
    latex_L.SetTextSize(0.050)
    latex_L.SetTextColor(ROOT.kRed)
    latex_L.Draw()
    latex_R = ROOT.TLatex( 0.20, 0.85, 'right side:   mean = %.2e, RMS = %.1f%%'%(mean_R,rms_R/mean_R*100.))
    latex_R.SetNDC()
    latex_R.SetTextSize(0.050)
    latex_R.SetTextColor(ROOT.kBlue)
    latex_R.Draw()
    latex_A = ROOT.TLatex( 0.20, 0.80, 'average:   mean = %.2e, RMS = %.1f%%'%(mean_A,rms_A/mean_A*100.))
    latex_A.SetNDC()
    latex_A.SetTextSize(0.050)
    latex_A.SetTextColor(ROOT.kBlack)
    latex_A.Draw()
    ROOT.gPad.SetGridy()
    c.Update()
    if not path or not filename:
        print('No path / filename specified; can not print pdf!')
    else:
        root, ext = os.path.splitext(filename)
        if not os.path.isdir("%s/%s"%(path,root)):
            os.mkdir("%s/%s"%(path,root))
        c.Print("%s/%s/%s.pdf" % (path, root, graphname))
        c.Print("%s/%s/%s.png" % (path, root, graphname))
