#!/usr/bin/env python3
import argparse
from pydantic import BaseModel
import pandas as pd
import openai
from tqdm import tqdm

class Output(BaseModel):
    matched: int

def parse_args():
    parser = argparse.ArgumentParser(description='Process matched authors data')
    parser.add_argument('--input', default='data/NSF/matched_authors.csv',
                       help='Input CSV file (default: data/NSF/matched_authors.csv)')
    return parser.parse_args()

def main():
    args = parse_args()
    # Main processing will go here
    df = pd.read_csv(args.input)
    print(df.head())
    df['verified'] = True
    batch_size = 20
    client = openai.OpenAI()
    for col1, col2 in [('author_name', 'csv_author_name')]:
        mismatch = []
        for index, row in df.iterrows():
            if row[col1] == row[col2]:
                continue
            mismatch.append(index)
        print(len(mismatch))
        for index in tqdm(mismatch):
            row = df.loc[index]
            prompt = f"Are {row[col1]} and {row[col2]} likely to be the same person? Return 1 for yes or 0 for no. If the names only differ by the presence of a middle name, you should return 1."
            completion = client.beta.chat.completions.parse(
                model="gpt-4o",
                messages=[
                    {"role": "user", "content": prompt},
                ],
                response_format=Output,
            )
            output = completion.choices[0].message.parsed        
            try:
                if output.matched == 0:
                    df.loc[index, 'verified'] = False
            except:
                print("-" * 100)
                print(prompt)
                print("=>")
                print(output)

    df.to_csv(args.input + '.verified.csv', index=False)


    
if __name__ == '__main__':
    main()

