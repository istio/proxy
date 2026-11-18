FROM python:3.11.18-trixie

ENV LC_ALL=C.UTF-8 \
    LANG=C.UTF-8 \
    LANGUAGE=C.UTF-8

RUN pip install cowsay

COPY --from=root . /app

COPY --from=bins_hi hi_/hi /usr/local/bin/hi

ENTRYPOINT ["/usr/local/bin/python3"]

CMD ["/app/say.py"]
