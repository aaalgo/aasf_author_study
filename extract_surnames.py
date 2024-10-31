#!/usr/bin/env python3
import json
import re
from bs4 import BeautifulSoup
import pinyin

# Load the HTML content from the file
with open("data/List_of_common_Chinese_surnames", "r", encoding="utf-8") as file:
    soup = BeautifulSoup(file, "html.parser")

names1 = set()
names2 = set()
wade_giles = set()
# Find the table with the specific class
for table in soup.find_all("table", {"class": "wikitable sortable"}):
    caption = table.find("caption")
    if not caption:
        continue
    caption = caption.get_text(strip=True)
    if caption == 'Romanizations':
        # Iterate over rows in the table body
        for row in table.find_all("tr"):
            columns = row.find_all("td")
            if len(columns) >= 4:
                rank = columns[0].get_text(strip=True)
                char = columns[1].get_text(strip=True)
                off = 3
                span = int(columns[1].get('colspan', 1))
                if span == 2:
                    off -= 1
                else:
                    char = columns[2].get_text(strip=True)
                    if span != 1:
                        print(span)
                        print(row)
                        assert False
                #pinyin = columns[off].get_text(strip=True)
                wade = columns[off+1].get_text(strip=True)
                wade_giles.add(re.sub(r'\d+', '', wade).lower())
                #names.append((char, pinyin, wade))
                names1.add(char)
    elif caption == '400 most common surnames in China':
        # Iterate over rows in the table body
        for row in table.find_all("tr"):
            columns = row.find_all("td")
            if len(columns) != 6:
                continue
            rank = columns[0].get_text(strip=True)
            char = columns[1].get_text(strip=True)
            #pinyin = columns[2].get_text(strip=True)
            names2.add(char)

roman = set(wade_giles)
for name in (names1 | names2):
    py = pinyin.get(name, format='strip', delimiter='')
    roman.add(py)

with open('data/surnames.json', 'w') as f:
    json.dump(list(roman), f)

