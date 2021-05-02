const arc = require('@architect/functions');

// learn more about HTTP functions here: https://arc.codes/primitives/http
exports.handler = async function http (req) {
  let body = '';
  let status = 200;
  //console.log(require('util').inspect(req, null, {maxDepth:-1}))
  try{
    const key = req.pathParameters.source;
    if(!key) throw "missing source";
    let data = await arc.tables();
      
    var params = {
      KeyConditionExpression: 'datasource = :hkey',
      ExpressionAttributeValues: {
        ':hkey': key
      }
    };

    const result = await data.rssi.query(params);
    console.log(result);
    body = JSON.stringify(result);
  } catch(e){
    console.log(e);
    body = JSON.stringify({error: e});
    status = 400;
  }

  return {
    statusCode: status,
    headers: {
      'cache-control': 'no-cache, no-store, must-revalidate, max-age=0, s-maxage=0',
      'content-type': 'application/json; charset=utf8'
    },
    body
  }
}