import os

try:
    os.remove('temp_data.csv')
except FileNotFoundError:
    pass
