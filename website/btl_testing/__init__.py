from flask import Flask

app = Flask(__name__)

app.config.from_envvar('WEBSITE_SETTINGS', silent=False)

# try to configure logging. It is extremely difficult to figure out what the
# proper way to log exceptions from Flask when running under gunicorn is. I
# initially tried the solution here:
# https://stackoverflow.com/questions/14037975/how-do-i-write-flasks-excellent-debug-log-message-to-a-file-in-production
# however I kept getting permission denied errors when trying to open the file
# even though I had set up the correct permissions on /var/log/minard.
# Eventually I started looking for another way to do it and found this post:
# https://medium.com/@trstringer/logging-flask-and-gunicorn-the-manageable-way-2e6f0b8beb2f
# which discusses how to pipe Flask's messages to the gunicorn logger.
@app.before_first_request
def setup_logging():
    if not app.debug:
        import logging
        gunicorn_logger = logging.getLogger('gunicorn.error')
        for handler in gunicorn_logger.handlers:
            app.logger.addHandler(handler)

import btl_testing.views
