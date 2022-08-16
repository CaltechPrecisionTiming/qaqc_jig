from __future__ import division, print_function
from . import app
from flask import render_template, jsonify, request, redirect, url_for, flash, make_response
import time
from os.path import join
import json
from .tools import parseiso, total_seconds
from collections import deque, namedtuple
from math import isnan
import os
import sys
import random
from .moduledb import get_channels, get_channel_info, ModuleUploadForm, upload_new_module, get_modules, get_module_info
from datetime import datetime
import pytz

@app.template_filter('timefmt')
def timefmt(timestamp):
    return time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime(float(timestamp)))

@app.errorhandler(500)
def internal_error(exception):
    return render_template('500.html'), 500

@app.route('/module-database')
def module_database():
    limit = request.args.get("limit", 100, type=int)
    offset = request.args.get("offset", 0, type=int)
    sort_by = request.args.get("sort-by", "timestamp")
    results = get_modules(request.args, limit, offset, sort_by)
    return render_template('module_database.html', results=results, limit=limit, offset=offset, sort_by=sort_by)

@app.route('/channel-database')
def channel_database():
    limit = request.args.get("limit", 100, type=int)
    offset = request.args.get("offset", 0, type=int)
    sort_by = request.args.get("sort-by", "timestamp")
    results = get_channels(request.args, limit, offset, sort_by)
    return render_template('channel_database.html', results=results, limit=limit, offset=offset, sort_by=sort_by)

@app.template_filter('time_from_now')
def time_from_now(dt):
    """
    Returns a human readable string representing the time duration between `dt`
    and now. The output was copied from the moment javascript library.

    See https://momentjs.com/docs/#/displaying/fromnow/
    """
    delta = total_seconds(datetime.now(pytz.timezone('US/Pacific')) - dt)

    if delta < 45:
        return "a few seconds ago"
    elif delta < 90:
        return "a minute ago"
    elif delta <= 44*60:
        return "%i minutes ago" % int(round(delta/60))
    elif delta <= 89*60:
        return "an hour ago"
    elif delta <= 21*3600:
        return "%i hours ago" % int(round(delta/3600))
    elif delta <= 35*3600:
        return "a day ago"
    elif delta <= 25*24*3600:
        return "%i days ago" % int(round(delta/(24*3600)))
    elif delta <= 45*24*3600:
        return "a month ago"
    elif delta <= 319*24*3600:
        return "%i months ago" % int(round(delta/(30*24*3600)))
    elif delta <= 547*24*3600:
        return "a year ago"
    else:
        return "%i years ago" % int(round(delta/(365.25*24*3600)))

@app.route("/upload-new-module", methods=["GET","POST"])
def upload_new_module_view():
    if request.form:
        form = ModuleUploadForm(request.form)
    else:
        form = ModuleUploadForm()

    if request.method == "POST" and form.validate():
        try:
            upload_new_module(form)
        except Exception as e:
            flash(str(e),'danger')
            return render_template("upload_new_module.html", form=form)
        flash("Successfully submitted", 'success')
        return redirect(url_for("upload_new_module_view"))

    return render_template("upload_new_module.html", form=form)

@app.route('/module-status')
def module_status():
    barcode = request.args.get("barcode", 0, type=int)
    run = request.args.get("run", None, type=int)
    info = get_module_info(barcode=barcode,run=run)
    if info is None:
        flash('No module found in database with that barcode. Did you forget to upload it?','danger')
        return redirect(url_for('module_database'))
    return render_template('module_status.html', info=info)

@app.route('/channel-status')
def channel_status():
    key = request.args.get("key", 0, type=int)
    info = get_channel_info(key=key)
    if info is None:
        flash('No channel found in database with that barcode. Did you forget to upload it?','danger')
        return redirect(url_for('channel_database'))
    return render_template('channel_status.html', info=info)

@app.route('/')
def index():
    return redirect(url_for('module_database'))
