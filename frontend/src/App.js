import './App.css';
import { Fish } from 'lucide-react';
import { Droplet } from 'lucide-react';
import { Cat } from 'lucide-react';
import { PawPrint } from 'lucide-react';
import firebase from './firebase';
import { useState, useEffect } from 'react';

function App() {
  // Static data - will be replaced with Firebase data later
  const [foodBowlAmount, setFoodBowlAmount] = useState(0); // grams
  const isConnected = true;
  const [waterLevel, setWaterLevel] = useState(0);
  const [refillLevel, setRefillLevel] = useState(0); // percentage
  const [nextSlot, setNextSlot] = useState(0);
  const [feedingLog, setFeedingLog] = useState([]);
  const [isFeeding, setIsFeeding] = useState(false);
  const [feedButtonText, setFeedButtonText] = useState('FEED NOW');

  // Fetch and sort feeding logs based on nextSlot
  // nextSlot is the next slot to write to, so most recent is nextSlot - 1
  const fetchAndSortSlots = (nextSlotValue) => {
    const slots = [];
    const promises = [];
    
    for (let i = 0; i < 4; i++) {
      promises.push(
        firebase.database().ref(`logs/slot_${i}`).once('value').then((snapshot) => {
          const data = snapshot.val();
          if (data && data.timestamp) {
            slots.push({
              type: data.type === 'auto' ? 'Auto Feed' : 'Manual Feed',
              timestamp: data.timestamp,
              slotIndex: i
            });
          }
        })
      );
    }
    
    Promise.all(promises).then(() => {
      // Sort by timestamp in descending order (newest first)
      // Parse timestamp strings "YYYY-MM-DD HH:MM:SS" to Date objects for comparison
      const sortedSlots = slots.sort((a, b) => {
        const dateA = new Date(a.timestamp.replace(' ', 'T'));
        const dateB = new Date(b.timestamp.replace(' ', 'T'));
        // Sort descending (newest first) - newest slot on top, others pushed down
        return dateB - dateA;
      });
      setFeedingLog(sortedSlots);
    });
  };

  useEffect(() => {
    const foodBowlRef = firebase.database().ref('data/weight');
    foodBowlRef.on('value', (snapshot) => {
      setFoodBowlAmount(snapshot.val());
    });

    const waterLevelRef = firebase.database().ref('data/water_level');
    waterLevelRef.on('value', (snapshot) => {
      setWaterLevel(snapshot.val());
    });

    const refillLevelRef = firebase.database().ref('data/refill_left');
    refillLevelRef.on('value', (snapshot) => {
      setRefillLevel(snapshot.val());
    });

    // Fetch nextSlot and update logs when it changes
    const nextSlotRef = firebase.database().ref('logs/logs_config/nextSlot');
    nextSlotRef.on('value', (snapshot) => {
      const newNextSlot = snapshot.val() || 0;
      setNextSlot(newNextSlot);
      fetchAndSortSlots(newNextSlot);
    });

    // Listen for changes in all slots
    const slotRefs = [];
    for (let i = 0; i < 4; i++) {
      const slotRef = firebase.database().ref(`logs/slot_${i}`);
      slotRef.on('value', () => {
        // Re-fetch when any slot changes
        firebase.database().ref('logs/logs_config/nextSlot').once('value').then((snapshot) => {
          const slot = snapshot.val() || 0;
          fetchAndSortSlots(slot);
        });
      });
      slotRefs.push(slotRef);
    }

    // Cleanup listeners
    return () => {
      foodBowlRef.off();
      waterLevelRef.off();
      refillLevelRef.off();
      nextSlotRef.off();
      slotRefs.forEach(ref => ref.off());
    };
  }, []);

  const handleFeedNow = () => {
    if (isFeeding) return; // Prevent multiple clicks
    
    // Set button to feeding state
    setIsFeeding(true);
    setFeedButtonText('FEEDING...');
    
    // Write to Firebase
    firebase.database().ref('manual/value').set(true);
    
    // Reset after 5 seconds
    setTimeout(() => {
      setIsFeeding(false);
      setFeedButtonText('FEED NOW');
      // Optionally set Firebase back to false
      firebase.database().ref('manual/value').set(false);
    }, 5000);
  };

  // Calculate inverse refill level (100 - refill_left)
  // Clamp values to ensure they're between 0 and 100 and handle null/undefined
  const safeRefillLevel = refillLevel ?? 10;
  const clampedRefillLevel = Math.max(10, Math.min(100, safeRefillLevel));
  let inverseRefillLevel = Math.max(10, Math.min(100, 100 - clampedRefillLevel));

  // Determine refill level color stage based on inverse (low remaining = red, high remaining = green)
  const getRefillColorStage = (inverseLevel) => {
    if (inverseLevel <= 25) return 'red';
    if (inverseLevel <= 50) return 'orange';
    if (inverseLevel <= 75) return 'yellow';
    return 'green';
  };

  // Determine water level status
  const getWaterLevelStatus = (level) => {
    // console.log("waterLevel", level, "%");
    if (level === 0) return 'EMPTY';
    if (level < 25) return 'LOW';
    if (level < 50) return 'OKAY';
    if (level < 75) return 'GOOD';
    return 'FULL';
  };

  const refillColorStage = getRefillColorStage(inverseRefillLevel);
  const waterLevelStatus = getWaterLevelStatus(waterLevel);

  return (
    <div className="App">
      {/* Header Section */}
      <div className="header-container">
        <div className="header-icon">
          <Cat size={100} color="#654321" strokeWidth={5} absoluteStrokeWidth />
        </div>
        <div className="header-text">
          <h1 className="main-title">Cat Ultimate Helper</h1>
          <p className="subtitle">C.U.H Automatic Feeder</p>
        </div>
        <div className="header-icon">
          <PawPrint size={100} color="#654321" strokeWidth={5} absoluteStrokeWidth />
        </div>
      </div>

      {/* Main Content */}
      <div className="main-content">
        {/* Top Row */}
        <div className="top-row">
          {/* Food Bowl Status */}
          <div className="card food-bowl-card">
            <h2 className="card-header">FOOD BOWL</h2>
            <div className="food-amount">
              <div className="food-amount-icon">
                <Fish size={100} color="#654321" strokeWidth={5} absoluteStrokeWidth />
              </div>
              <span className="amount-value">{foodBowlAmount}</span>
              <span className="amount-unit">grams</span>
            </div>
          </div>

          {/* Feeding Log */}
          <div className="card feeding-log-card">
            <h2 className="card-header">FEEDING LOG</h2>
            <div className="feeding-log-list">
              {feedingLog.length === 0 ? (
                <div className="feeding-log-entry">
                  <div className="log-type">NO LOGS YET</div>
                </div>
              ) : (
                feedingLog.map((entry, index) => (
                  <div key={index} className="feeding-log-entry">
                    <div className="log-type">{entry.type}</div>
                    <div className="log-time">{entry.timestamp}</div>
                  </div>
                ))
              )}
            </div>
          </div>

          {/* Connection Status and Water Level */}
          <div className="right-column">
            {/* Connection Status */}
            {/* <div className="card connection-status-card">
              <span className="connection-text">Connected</span>
              <span className={`connection-indicator ${isConnected ? 'connected' : 'disconnected'}`}></span>
            </div> */}

            {/* Water Level */}
            <div className="card water-level-card">
              <h2 className="card-header">WATER LEVEL</h2>
              <div className="water-indicator">
                <div className="water-droplet">
                  <Droplet size={100} color="#654321" strokeWidth={5} absoluteStrokeWidth />
                </div>
                <div className="water-status">{waterLevelStatus}</div>
              </div>
            </div>
          </div>
        </div>

        {/* Bottom Row */}
        <div className="bottom-row">
          {/* Feed Now Button */}
          <button 
            className={`card feed-now-button ${isFeeding ? 'feeding' : ''}`}
            onClick={handleFeedNow}
            disabled={isFeeding}
          >
            {feedButtonText}
          </button>

          {/* Refill Level Indicator */}
          <div className="card refill-level-card">
            <span className="refill-text">REFILL LEVEL</span>
            <div className="refill-progress-bar">
              <div 
                className={`refill-progress-fill refill-${refillColorStage}`}
                style={{ width: `${inverseRefillLevel}%` }}
              ></div>
            </div>
          </div>
        </div>
      </div>

      <div className="footer-container">
        <div className="footer-content">
          <h3 className="footer-title">Members</h3>
          <div className="members-list">
            <div className="member-item">
              <span className="member-name">Parkorn Wattanasukchai</span>
              <span className="member-id">6631340221</span>
            </div>
            <div className="member-item">
              <span className="member-name">Weeraphat Kawthaisong</span>
              <span className="member-id">6631348321</span>
            </div>
            <div className="member-item">
              <span className="member-name">Sukon Lapprasert</span>
              <span className="member-id">6631351121</span>
            </div>
            <div className="member-item">
              <span className="member-name">Ittichet Thongsang</span>
              <span className="member-id">6631363721</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}

export default App;
