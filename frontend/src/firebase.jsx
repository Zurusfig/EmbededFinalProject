import firebase from "firebase/compat/app";
import "firebase/compat/database";

const firebaseConfig = {
  apiKey: "AIzaSyAMvydTgXJLog3_H5LAw1o8xnhHP2aTAHQ",
  authDomain: "embeded-finalproject.firebaseapp.com",
  databaseURL: "https://embeded-finalproject-default-rtdb.asia-southeast1.firebasedatabase.app",
  projectId: "embeded-finalproject",
  storageBucket: "embeded-finalproject.firebasestorage.app",
  messagingSenderId: "462930566400",
  appId: "1:462930566400:web:c17bc497b79819933aa7e5",
  measurementId: "G-Q2GQRVYC6S"
};

firebase.initializeApp(firebaseConfig);
export default firebase;