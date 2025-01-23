#!/usr/bin/env python3
import os
import json
import numpy as np
import matplotlib.pylab as plt

LABELS = [
    #'All',
    'Engineering and Computer Science',
    'Physical Sciences',
    'Life Sciences',
    'Social Sciences'
]

COUNTRIES = ['US','CN','IN','CA','DE','FR','AU','KR','JP','CH','UK','other'];

ORDER_COUNTRIES = ['CN','IN','CA','DE','FR','AU','KR','JP','CH', 'UK']

lookup = {}
for i, v in enumerate(COUNTRIES):
    lookup[v] = i

ORDER = [lookup[v] for v in ORDER_COUNTRIES]

del lookup

# npy
# is_chinese
# year
# destination
class Analyze:
    def __init__ (self, root, year_begin, year_end):
        self.root = root
        with open(os.path.join(root, 'meta.json'), 'r') as f:
            meta = json.load(f)
        self.meta = meta
        counts = np.load(os.path.join(root, 'counts.npy'))
        assert counts.shape[-1] == len(COUNTRIES)
        print(meta)
        print(counts.shape)
        print(np.sum(counts))
        self.offset = meta['year_begin']
        begin = year_begin - self.offset    
        end = year_end - self.offset
        self.counts = counts[:, :, begin:end, :]
        self.year_begin = year_begin
        self.year_end = year_end
        self.X = np.arange(year_begin, year_end)        

    def plot_chinese_to_china (self):
        plt.figure()
        plt.title("Chinese Scholar to China")
        for label in LABELS:
            for i, domain in enumerate(self.meta['domains']):
                if label == domain['display_name']:
                    plt.plot(self.X, self.counts[i, 1, :, 1], label=label)
                    break
        plt.legend()
        plt.savefig(f'{self.root}/chinese_to_china.png')

    def plot_chinese_vs_non_chinese (self):
        plt.figure()
        plt.title("Chinese vs Non-Chinese to China")
        plt.plot(self.X, self.counts[1, 0, :, 1], label="Chinese")
        plt.plot(self.X, self.counts[0, 0, :, 1], label="Non-Chinese")        
        plt.plot(self.X, self.counts[2, 0, :, 1], label="Both")        
        plt.legend()
        plt.savefig(f'{self.root}/chinese_vs_non_chinese.png')

    def plot_all_to_destinations (self, yesno, title):
        plt.figure()
        plt.title(title)
        if yesno is None:
            #counts = np.sum(self.counts, 1)
            counts = self.counts[:, 2, :, :]
        else:
            counts = self.counts[:, yesno, :, :]
        # Use a colormap with at least 11 distinct colors
        colors = plt.cm.tab20(np.linspace(0, 1, len(ORDER)))
        for idx, i in enumerate(ORDER):
            dest = COUNTRIES[i]
            plt.plot(self.X, counts[0, :, i], label=dest, color=colors[idx])
        plt.legend()        
        title_r = title.replace(' ', '_')
        plt.savefig(f'{self.root}/{title_r}.png')

    def plot_year_make_up (self, year):
        year_offset = year - self.X[0]

        #counts.resize({domains.size(), 3, TOTAL_YEARS, NUM_COUNTRIES});
        for i, domain in enumerate(self.meta['domains']):
            outflow = self.counts[i, 2, year_offset, :]
            assert outflow.shape[0] == len(COUNTRIES)
            X = COUNTRIES[1:]
            Y = outflow[1:]
            plt.figure()
            plt.title(f"Distribution of Destinations, {year}")
            colors = plt.cm.tab20(np.linspace(0, 1, len(Y)))
            plt.pie(Y, labels=X, colors=colors)
            plt.savefig(f'{self.root}/pie_{domain["display_name"].replace(" ", "_")}_{year}.png')

ROOT = '20250108_outflow'
#ROOT = 'from_NIH/outflow'
ROOT = 'data/new/outflow'
        
anal = Analyze(ROOT, 2010, 2024)
anal.plot_chinese_to_china()
anal.plot_chinese_vs_non_chinese()
anal.plot_all_to_destinations(None, "All Different Destinations")
anal.plot_all_to_destinations(0, "Non-Chinese Different Destinations")
anal.plot_all_to_destinations(1, "Chinese Different Destinations")
anal.plot_year_make_up(2022)
anal.plot_year_make_up(2023)
