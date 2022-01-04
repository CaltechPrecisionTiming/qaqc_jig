default: build

INSTALL=install

.DEFAULT:
	./setup.py $@

install:
	mkdir -p /opt/btl-testing
	mkdir -p /var/log/gunicorn
	chown nginx:nginx /var/log/gunicorn
	rsync -a --chown=nginx:nginx . /opt/btl-testing
	$(INSTALL) gunicorn.socket /etc/systemd/system
	$(INSTALL) gunicorn.service /etc/systemd/system
	$(INSTALL) nginx.conf /etc/nginx/nginx.conf
	systemctl enable --now gunicorn.socket
	systemctl enable --now gunicorn

.PHONY: install
