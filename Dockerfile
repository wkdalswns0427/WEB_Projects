FROM node:16.20.1

RUN mkdir -p /app

WORKDIR /app

COPY package*.json /app/

RUN npm install

COPY app.js /app/
COPY page.html /app/
# COPY .env /app/
COPY ./public/ /app/public/

EXPOSE 6060
CMD [ "node", "app.js" ]