import csv
import os
import sys

def organize_data(raw, output):
    if raw['Data Type'] == 'SPE':
        output['SPE Charge'] = raw['Charge']
        output['SPE Charge Err Abs'] = raw['Err Abs']
    elif raw['Data Type'] == 'sodium':
        output['511 Peak Charge'] = raw['Charge']
        output['511 Peak Charge Err Abs'] = raw['Err Abs']

if __name__ == '__main__':
    ENERGY = 0.511
    data = []

    from argparse import ArgumentParser

    parser = ArgumentParser()
    parser.add_argument('--BV', type=float, help='SiPM Bias Voltage')
    # parser.add_argument('--jp', required=True, type=int, help='jumper position on the PCB board')
    # parser.add_argument('--sipm_num', required=True, type=int, help='SiPM Number')
    parser.add_argument('--n', type=int, help='Number of events')
    parser.add_argument('--date', help='Date data was taken')
    parser.add_argument('--time', help='Time data was taken')
    parser.add_argument('--extra', help='Any notes about this set of data')
    parser.add_argument('--source', help='CAEN or SCOPE')
    parser.add_argument('-o', '--output', default='Fit_Data.csv', help='output csv filename')
    args = parser.parse_args()

    try: 
        with open('temp_data.csv', 'r') as file:
            reader = csv.DictReader(file)
            for row in reader:
                data.append(row)
    except FileNotFoundError:
        print("No data to save to csv")
        exit()
    
    with open(args.output, 'a', newline='') as file:
        headers = [
            'Channel',
            'SPE Charge',
            'SPE Charge Err Abs',
            '511 Peak Charge',
            '511 Peak Charge Err Abs',
            'LO',
            'LO Under Err',
            'LO Over Err',
            'Bias',
            'Events',
            'Date',
            'Time',
            'Source',
            # 'SiPM Number',
            # 'Jumper Position',
            'Extra'
        ]
        
        writer = csv.DictWriter(file, headers)
        if os.stat(args.output).st_size == 0:
            writer.writeheader()
        
        rows = {}
        for chunk in data:
            if not chunk['Channel'] in rows.keys():
                rows[chunk['Channel']] = {'Channel': chunk['Channel']}
            organize_data(chunk, rows[chunk['Channel']])
        for channel in rows.values():
            if channel:
                if '511 Peak Charge' in channel.keys():
                    peak = float(channel['511 Peak Charge'])
                    peakerr = float(channel['511 Peak Charge Err Abs'])
                if 'SPE Charge' in channel.keys():
                    spe = float(channel['SPE Charge'])
                    speerr = float(channel['SPE Charge Err Abs'])
                if '511 Peak Charge' in channel.keys() and 'SPE Charge' in channel.keys():
                    channel['LO'] =  peak / spe / ENERGY
                    channel['LO Under Err'] = ((peak - peakerr) / (spe + speerr)) / ENERGY - channel['LO']
                    channel['LO Over Err'] = ((peak + peakerr) / (spe - speerr)) / ENERGY - channel['LO']
                # for i in range(ch_spec_args+2, len(sys.argv)-1, 2):
                #     channel[sys.argv[i]] = sys.argv[i+1]

        # if 'channel1' in rows.keys():
        #     rows['channel1']['SiPM Number'] = sys.argv[2]
        #     rows['channel1']['Jumper Position'] = sys.argv[3]
        # if 'channel2' in rows.keys():
        #     rows['channel2']['SiPM Number'] = sys.argv[4]
        #     rows['channel2']['Jumper Position'] = sys.argv[5]
       
        for channel in rows.values():
            channel['Bias'] = args.BV
            channel['Events'] = args.n
            channel['Date'] = args.date
            channel['Time'] = args.time
            channel['Source'] = args.source
            channel['Extra'] = args.extra
            writer.writerow(channel)

        os.remove('temp_data.csv')

    
        
