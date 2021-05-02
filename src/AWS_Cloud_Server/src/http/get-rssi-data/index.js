const arc = require('@architect/functions');

exports.handler = async function http (req) {
  let body = '';
  let status = 200;
  
  try{
    const {source, id} = req.query;
    
    if(!source || !id) throw "missing parameters";

    let data = await arc.tables();
      
    var params = {
      source, sampleid: id
    };

    const result = data.rssi.get(params);
    body = JSON.stringify(result.Attributes);
  } catch(e){
    body = JSON.stringify({error: e});
    status = 400;
  }


  return {
    statusCode,
    headers: {
      'cache-control': 'no-cache, no-store, must-revalidate, max-age=0, s-maxage=0',
      'content-type': 'text/html; charset=utf8'
    },
    body
  }
}