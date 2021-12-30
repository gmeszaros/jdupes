FROM gcc:bullseye as builder

COPY . .
RUN make && make install

FROM debian:bullseye-slim as runner

COPY --from=builder /usr/local/bin/jdupes /usr/local/bin/jdupes
# COPY --from=builder /usr/local/share/man/man1/jdupes.1 /usr/local/share/man/man1/jdupes.1 

ENTRYPOINT [ "/usr/local/bin/jdupes" ]
