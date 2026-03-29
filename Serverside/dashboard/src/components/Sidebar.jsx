import { Link } from "react-router-dom";

export default function Sidebar() {
  return (
    <div style={{ width: "200px", background: "#eee", padding: "20px" }}>
      <h2>Menu</h2>
      <ul>
        <li><Link to="/">Dashboard</Link></li>
        <li><Link to="/settings">Settings</Link></li>
      </ul>
    </div>
  );
}