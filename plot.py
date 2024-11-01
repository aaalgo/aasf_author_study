#!/usr/bin/env python3
import os
import json
import numpy as np
import pandas as pd
import matplotlib.pylab as plt

LABELS = [
    # label, [(name, level)], color
    #        ^ None means [(label, 'domain')]
    ('Physical Sciences', 'green'),
    ('Engineering and Computer Science', 'blue'),
    ('Life Sciences', 'orange'),
    ('Social Sciences', 'red')
]

def plot_old (root):
    df = pd.read_csv('data/Chinese_descent_scientists_destination_count.csv')
    for kind in ['All', 'Experienced', 'Junior']:
        plt.figure()
        for domain, color in [('Life science', 'orange'),
                              ('Formal and physical science', 'green'),
                              ('Engineering and computer science', 'blue'),
                              ('Social sciences', 'red')]:
            col = f'{domain}={kind}'
            plt.plot(df['Year'], df[col], label=domain, color=color)
        plt.title(kind)
        plt.legend()
        plt.xlim(2010, 2021)
        plt.savefig(os.path.join(root, f'old_{kind}.png'))

def analyze (root, year_begin, year_end):
    plot_old(root)
    plt.figure()
    with open(os.path.join(root, 'meta.json'), 'r') as f:
        meta = json.load(f)
    counts = np.load(os.path.join(root, 'counts.npy'))
    print(meta)
    print(counts.shape)
    offset = meta['year_begin']
    begin = year_begin - offset    
    end = year_end - offset
    X = np.arange(year_begin, year_end)
    for label, color in LABELS:
        cc = None
        for i, domain in enumerate(meta['domains']):
            if domain['display_name'] == label:
                cc = counts[i]
                break
        assert not cc is None
        plt.plot(X, cc[begin:end], label=label, color=color)
    plt.legend()
    plt.savefig(os.path.join(root, 'plot.png'))

analyze('count', 2010, 2024)
