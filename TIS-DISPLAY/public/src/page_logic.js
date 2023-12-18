function createCard(OrderNo, TableId,order_products,total_price, order_time) {
    const cardWrap = document.createElement('div');
    cardWrap.className = 'card-wrap'; // .card 태그 생성

    const cardHeader = document.createElement('div');
    cardHeader.className = 'card-header'; // .card-inner 태그 생성
    var fab = document.createElement('i');
    fab.className = 'fab fa-grav'
    var cardNumber = document.createElement('h1');
    cardNumber.className = 'card-num';
    cardNumber.innerText = Number(OrderNo)+1;
    cardNumber.style.position = "relative"
    cardNumber.style.top = "5px"
    cardNumber.style.right = "160px"
    cardNumber.style.color = "antiquewhite"

    var orderTime = document.createElement('p');
    orderTime.style.position = "relative"
    orderTime.style.top = "-15px"
    orderTime.style.left = "100px"
    orderTime.style.color = "antiquewhite"
    orderTime.innerText = order_time

    cardHeader.appendChild(fab)
    cardHeader.appendChild(cardNumber)
    cardHeader.appendChild(orderTime)
    cardWrap.appendChild(cardHeader)

    const cardContent = document.createElement('div');
    cardContent.className = 'card-content'; // .card-front 태그 생성

    // Table ID on top
    var cardTitle = document.createElement('h1');
    cardTitle.className = 'card-title';
    cardTitle.style.fontSize = "30px"
    cardTitle.innerText = TableId;
    cardContent.appendChild(cardTitle);

    // Ordered Menu, Options
    length_op = order_products.length
    for(var i = 0; i < length_op; i++){
      cardContent.appendChild(makeOrderContent(order_products[i]))
    }

    // Order Price
    var cardPrice = document.createElement('footer');
    cardPrice.className = 'card-price';
    cardPrice.style.position = "absolute"
    cardPrice.style.bottom = "300px"
    cardPrice.style.fontSize = "20px"
    cardPrice.style.fontWeight = "bold"
    cardPrice.innerText = total_price + " 원"

    var priceline = document.createElement('HR');

    // cardContent.appendChild(cardText);
    cardContent.appendChild(priceline);
    cardContent.appendChild(cardPrice);
    cardWrap.appendChild(cardContent);
    document.body.append(cardWrap)
  }

function makeOrderContent(order_product){
  var cardText = document.createElement('details');
  var cardOptions = document.createElement('summary');
  cardText.appendChild(cardOptions)
  cardText.className = 'card-text';
  cardOptions.style.fontSize = "24px"
  cardOptions.style.fontWeight = "bold"
  cardOptions.innerText = order_product.product_name

  for(var i = 0; i < order_product.product_options.length; i++){
    var optionContents = document.createElement('p');
    optionContents.innerText = order_product.product_options[i].sub_product_name
    cardText.appendChild(optionContents)
  }
  cardText.style.fontSize = "20px"
  cardText.style.position = "relative"
  cardText.style.textAlign = "left"

  // cardText.innerText

  return cardText
}

window.onload=function makeContent(){
  data = parseData()
  val = overallData()
}

async function parseData(){
  const list = await fetchData();
  contents = list.data
  length = contents.length
  // console.log(contents)

  for(var i = 0; i < length; i++){
    createCard(i,contents[i].table_id, contents[i].order_products, contents[i].total_price, contents[i].order_time)
  }
}

function fetchData() {
  return new Promise((resolve, reject) => {
    fetch('http://192.168.12.141:7003/api/orders/board?total_order_done_flag=false')
      .then(response => response.json())
  
      .then(data => {
        resolve(data);
      });
    });
  }

async function overallData(){
  const element = document.getElementById('overalldata');
  const list = await fetchoverallData();
  // console.log(Number(list.total_amount))
  element.innerHTML=Number(list.total_amount)
  return Number(list.total_amount)
}

function fetchoverallData() {
  return new Promise((resolve, reject) => {
    fetch('http://192.168.12.141:7004/report/overall?period=day')
      .then(response => response.json())
  
      .then(data => {
        resolve(data);
      });
    });
  }

function refreshPage(){
  window.location.reload();
}