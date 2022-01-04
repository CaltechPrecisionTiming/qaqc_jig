import sqlalchemy
from .views import app

engine = sqlalchemy.create_engine('postgresql://%s:%s@%s:%i/%s' %
                                 (app.config['DB_USER'], app.config['DB_PASS'],
                                  app.config['DB_HOST'], app.config['DB_PORT'],
                                  app.config['DB_NAME']),
                                  pool_recycle=3600)
