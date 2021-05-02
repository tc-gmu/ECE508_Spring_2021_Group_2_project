@app
ece508

@aws
region us-east-1

@http
post /rssi/data
get  /rssi/source/:source

#get  /rssi/data
#get  /rssi/overview

@tables
rssi
  datasource *String
  sampleid **String
  expires TTL
