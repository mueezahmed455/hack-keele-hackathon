import Card from "../components/Card";

export default function Dashboard() {
  return (
    <div>
      <h1>Dashboard</h1>

      <div style={{ display: "flex", gap: "20px" }}>
        <Card title="Users" value="1,024" />
        <Card title="Revenue" value="$12,340" />
      </div>
    </div>
  );
}