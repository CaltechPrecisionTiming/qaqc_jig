from .db import engine
from .views import app
import psycopg2
import psycopg2.extensions
from wtforms import Form, validators, IntegerField, SelectField, PasswordField, TextAreaField

class ModuleUploadForm(Form):
    """
    A class for the form to upload a new module.
    """
    barcode = IntegerField('Barcode', [validators.NumberRange(min=0,max=100000)])
    sipm = SelectField('SiPM Type', choices=[('HPK','HPK'),('FBK','FBK')])
    institution = SelectField('Assembly Institution', choices=[('Caltech','Caltech'),('UVA','UVA'),('Rome','Rome')])
    comments = TextAreaField('Comments', [validators.optional(), validators.length(max=10000)])
    password = PasswordField('Password')

def upload_new_module(form):
    """
    Upload a new module to the database.
    """
    conn = psycopg2.connect(dbname=app.config['DB_NAME'],
                            user=app.config['DB_BTL_USER'],
                            host=app.config['DB_HOST'],
                            password=form.password.data)
    conn.set_isolation_level(psycopg2.extensions.ISOLATION_LEVEL_AUTOCOMMIT)

    cursor = conn.cursor()
    cursor.execute("INSERT INTO modules (barcode, sipm, institution, comments) VALUES (%s, %s, %s, %s)", (form.data['barcode'], form.data['sipm'], form.data['institution'], form.data['comments']))
    print(cursor.statusmessage)

def get_module_info(barcode, run=None):
    conn = engine.connect()

    query = "SELECT *, runs.institution as runs_institution, modules.timestamp as modules_timestamp, modules.institution as modules_institution, runs.timestamp as timestamp FROM (SELECT run, barcode, array_agg(key ORDER BY channel) as keys, array_agg(data.channel ORDER BY channel) as channels, array_agg(data.sodium_peak ORDER BY channel) as sodium_peak, array_agg(spe ORDER BY channel) as spe FROM data GROUP BY (run,barcode)) as channel, runs, modules WHERE channel.run = runs.run AND channel.barcode = modules.barcode"

    if run is not None:
        vars = (barcode, run)
        query += " AND channel.barcode = %s AND runs.run = %s"
    else:
        vars = (barcode,)
        query += " AND channel.barcode = %s"

    query += " ORDER BY runs.timestamp DESC"

    result = conn.execute(query,vars)

    keys = result.keys()
    row = result.fetchone()

    if row is None:
        # No module info?
        query = "SELECT *, runs.institution as runs_institution, runs.timestamp as timestamp FROM (SELECT run, barcode, array_agg(key ORDER BY channel) as keys, array_agg(data.channel ORDER BY channel) as channels, array_agg(data.sodium_peak ORDER BY channel) as sodium_peak, array_agg(spe ORDER BY channel) as spe FROM data GROUP BY (run,barcode)) as channel, runs WHERE channel.run = runs.run"

        if run is not None:
            vars = (barcode, run)
            query += " AND channel.barcode = %s AND runs.run = %s"
        else:
            vars = (barcode,)
            query += " AND channel.barcode = %s"

        query += " ORDER BY runs.timestamp DESC"

        result = conn.execute(query,vars)

        keys = result.keys()
        row = result.fetchone()

        if row is None:
            return None

    return dict(zip(keys,row))

def get_channel_info(key):
    conn = engine.connect()

    query = "SELECT *, runs.institution as runs_institution, modules.timestamp as modules_timestamp, modules.institution as modules_institution, data.timestamp as timestamp FROM data, modules, runs WHERE data.barcode = modules.barcode AND data.run = runs.run AND key = %s"

    result = conn.execute(query, (key,))

    keys = result.keys()
    row = result.fetchone()

    if row is None:
        # Maybe no module uploaded?
        query = "SELECT *, runs.institution as runs_institution, data.timestamp as timestamp FROM data, runs WHERE data.run = runs.run AND key = %s"

        result = conn.execute(query, (key,))

        keys = result.keys()
        row = result.fetchone()

    return dict(zip(keys,row))

def get_modules(kwargs, limit=100, offset=0, sort_by=None):
    """
    Returns a list of the latest data for individual channels. `kwargs` should
    be a dictionary containing fields and their associated values to select on.
    For example, to select only channels that have low occupancy:

        >>> get_channels({'low_occupancy': True})

    `limit` should be the maximum number of records returned.
    """
    conn = engine.connect()

    query = "SELECT * FROM (SELECT min(timestamp) as timestamp, run, barcode FROM data GROUP BY (run, barcode)) as channel, runs WHERE channel.run = runs.run"

    if sort_by == 'timestamp':
        query += " ORDER BY runs.timestamp DESC LIMIT %i OFFSET %i" % (limit,offset)
    else:
        query += " ORDER BY runs.timestamp DESC LIMIT %i OFFSET %i" % (limit,offset)

    result = conn.execute(query, kwargs)

    if result is None:
        return None

    keys = result.keys()
    rows = result.fetchall()

    return [dict(zip(keys,row)) for row in rows]

def get_channels(kwargs, limit=100, offset=0, sort_by=None):
    """
    Returns a list of the latest data for individual channels. `kwargs` should
    be a dictionary containing fields and their associated values to select on.
    For example, to select only channels that have low occupancy:

        >>> get_channels({'low_occupancy': True})

    `limit` should be the maximum number of records returned.
    """
    conn = engine.connect()

    conditions = []
    for key in kwargs:
        if key == 'barcode':
            conditions.append("%s = %%(%s)s" % (key, key))

    query = "SELECT * FROM data"
    if len(conditions):
        query += " WHERE %s " % (" AND ".join(conditions))

    if sort_by == 'timestamp':
        query += " ORDER BY data.timestamp DESC LIMIT %i OFFSET %i" % (limit,offset)
    else:
        query += " ORDER BY data.timestamp DESC LIMIT %i OFFSET %i" % (limit,offset)

    result = conn.execute(query, kwargs)

    if result is None:
        return None

    keys = result.keys()
    rows = result.fetchall()

    return [dict(zip(keys,row)) for row in rows]
