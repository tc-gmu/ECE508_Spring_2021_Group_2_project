const uuid = require('uuid');
const arc = require('@architect/functions');
const requestIp = require('request-ip');

let OPTIONAL_DATA_FIELDS = 

exports.handler = async function http (req) {
  let headers = {
    'cache-control': 'no-cache, no-store, must-revalidate, max-age=0, s-maxage=0',
    'content-type': 'application/json; charset=utf8'
  };
  //console.log(require('util').inspect(req, null, {maxDepth:-1}))

  let pb;

  try{
    if(req.isBase64Encoded === true)  pb = btoa(req.body);
    else                              pb = req.body;

    console.log(pb);

    pb = JSON.parse(pb);
  
  } catch( e ){
    console.log("could not parse body: ", e);
  } 


   let statusCode = 201;
  let body = '';

  let data = await arc.tables();
  let expires = Date.now()/1000 + (60*60*24*30);

  let rec = {
    sampleid: uuid.v1(),
    datasource: pb.source || requestIp.getClientIp(req),
    expires 
  };// expire in 30 days

  Object.assign(pb, rec);

  try{

    let result = await data.rssi.put(pb);
    
    body = JSON.stringify({
      id: rec.sampleid,
      source: rec.datasource
    });
  } catch (e){
    console.log(e);
    body = JSON.stringify({error: e, entry: rec});
    statusCode = 400;
  }

  return {
    statusCode,
    headers,
    body
  };

}