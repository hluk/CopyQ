FROM ubuntu:18.04
LABEL \
    name="CopyQ Launchpad Environment" \
    description="Enviroment for submitting CopyQ to Launchpad" \
    vendor="Lukas Holecek <hluk@email.cz>" \
    license="GPLv2+"

RUN apt-get update && apt install -y \
  debhelper \
  devscripts \
  dpkg-dev \
  dput \
  git \
  gnupg2 \
  vim

# gpg2 --export-secret-keys $EMAIL > launchpad.key
COPY launchpad.key /tmp/gpg.key

# cp ~/.gitconfig .
COPY .gitconfig /root/.gitconfig

RUN git clone https://github.com/hluk/CopyQ.git /root/copyq

WORKDIR /root/copyq

COPY entrypoint.sh /root/entrypoint.sh
ENTRYPOINT ["/root/entrypoint.sh"]
