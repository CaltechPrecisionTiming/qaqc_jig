#!/usr/bin/env python
from setuptools import setup
from glob import glob

setup(name='website',
      version='0.1',
      description='CMS BTL Module Testing Website',
      author='Anthony LaTorre',
      author_email='alatorre@caltech.edu',
      packages=['website'],
      include_package_data=True,
      zip_safe=False,
      scripts=glob('bin/*'),
      install_requires=['flask',
                        'gunicorn',
                        'numpy',
                        'argparse',
                        'sqlalchemy',
                        'psycopg2',
                        'alabaster',
                        'pytz']
      )
