FROM centos/httpd-24-centos7
USER root

COPY app_data /opt/app-root/src
RUN chown -R 1000490000:1000490000 /opt/app-root/src

EXPOSE 8080 

USER 1001
ENTRYPOINT ["/usr/bin/run-httpd"]
