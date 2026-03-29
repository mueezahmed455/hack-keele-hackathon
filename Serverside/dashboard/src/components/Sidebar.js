import { Link } from "react-router-dom";

export default function Sidebar() {
  return (
    <div style={{ width: "200px", background: "#f4f4f4", padding: "20px" }}>
      <h2>Menu</h2>

      <ul style={{ listStyle: "none", padding: 0 }}>
        <li><Link to="/">Dashboard</Link></li>
        <li><Link to="/settings">Settings</Link></li>
      </ul>
    </div>
  );
}