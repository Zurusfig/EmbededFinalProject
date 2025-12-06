import './App.css';
import { Fish } from 'lucide-react';
import { Droplet } from 'lucide-react';
import { Cat } from 'lucide-react';
import { PawPrint } from 'lucide-react';

function App() {
  // Static data - will be replaced with Firebase data later
  const foodBowlAmount = 200; // grams
  const isConnected = true;
  const waterLevel = "EMPTY!!";
  const refillLevel = 15; // percentage
  
  const feedingLog = [
    { type: "Manual Feed", time: "2:30pm", date: "10/12/25" },
    { type: "Auto Feed", time: "5:30am", date: "10/12/25" },
    { type: "Auto Feed", time: "0:00am", date: "10/12/25" },
    { type: "Auto Feed", time: "9:30pm", date: "9/12/25" },
  ];

  const handleFeedNow = () => {
    // Will be implemented with Firebase later
    console.log("Feed Now clicked");
  };

  // Determine refill level color stage
  const getRefillColorStage = (level) => {
    if (level <= 25) return 'red';
    if (level <= 50) return 'orange';
    if (level <= 75) return 'yellow';
    return 'green';
  };

  const refillColorStage = getRefillColorStage(refillLevel);

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
              {feedingLog.map((entry, index) => (
                <div key={index} className="feeding-log-entry">
                  <div className="log-type">{entry.type}</div>
                  <div className="log-time">{entry.time} {entry.date}</div>
                </div>
              ))}
            </div>
          </div>

          {/* Connection Status and Water Level */}
          <div className="right-column">
            {/* Connection Status */}
            <div className="card connection-status-card">
              <span className="connection-text">Connected</span>
              <span className={`connection-indicator ${isConnected ? 'connected' : 'disconnected'}`}></span>
            </div>

            {/* Water Level */}
            <div className="card water-level-card">
              <h2 className="card-header">WATER LEVEL</h2>
              <div className="water-droplet">
                <Droplet size={100} color="#654321" strokeWidth={5} absoluteStrokeWidth />
              </div>
              <div className="water-status">{waterLevel}</div>
            </div>
          </div>
        </div>

        {/* Bottom Row */}
        <div className="bottom-row">
          {/* Feed Now Button */}
          <button className="card feed-now-button" onClick={handleFeedNow}>
            FEED NOW
          </button>

          {/* Refill Level Indicator */}
          <div className="card refill-level-card">
            <span className="refill-text">REFILL LEVEL</span>
            <div className="refill-progress-bar">
              <div 
                className={`refill-progress-fill refill-${refillColorStage}`}
                style={{ width: `${refillLevel}%` }}
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
