FROM devkitpro/devkitarm@sha256:116afba8df8453961de2936ffab20dd441edf4d682856c1ec8b0e53d7ed0bbf5

RUN git clone https://github.com/profi200/dma330as && \
    cd dma330as && \
    make && \
    cp dma330as /usr/local/bin/ && \
    cd .. && \
    git clone --depth 1 --recurse-submodules https://github.com/derrekr/ctr_firm_builder.git && \
    cd ctr_firm_builder && \
    make && \
    cp firm_builder /usr/local/bin/ && \
    cd .. && \
    rm -rf dma330as/ ctr_firm_builder/ && \
    apt-get install -y --no-install-recommends p7zip-full && \
    apt-get -y autoremove --purge && \
    apt-get -y clean
