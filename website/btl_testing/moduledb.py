from .db import engine
from .views import app
import psycopg2
import psycopg2.extensions

def get_module_info(key):
    conn = engine.connect()

    query = "SELECT *, modules.institution as modules_institution, btl_qa.institution as btl_qa_institution, modules.timestamp as modules_timestamp, btl_qa.timestamp as btl_qa_timestamp FROM btl_qa, modules WHERE key = %s AND btl_qa.barcode = modules.barcode"

    result = conn.execute(query, (key,))

    if result is None:
        return None

    keys = result.keys()
    row = result.fetchone()

    print(keys)
    print(dir(result))
    return dict(zip(keys,row))

def get_modules(kwargs, limit=100, sort_by=None):
    """
    Returns a list of the current channel statuses for multiple channels in the
    detector. `kwargs` should be a dictionary containing fields and their
    associated values to select on. For example, to select only channels that
    have low occupancy:

        >>> get_channels({'low_occupancy': True})

    `limit` should be the maximum number of records returned.
    """
    conn = engine.connect()

    conditions = []
    for key in kwargs:
        conditions.append("%s = %%(%s)s" % (key, key))

    query = "SELECT * FROM btl_qa "
    if len(conditions):
        query += "WHERE %s " % (" AND ".join(conditions))

    if sort_by == 'timestamp':
        query += "ORDER BY timestamp DESC LIMIT %i" % limit
    else:
        query += "ORDER BY crate, slot, channel LIMIT %i" % limit

    result = conn.execute(query, kwargs)

    if result is None:
        return None

    keys = result.keys()
    rows = result.fetchall()

    return [dict(zip(keys,row)) for row in rows]
