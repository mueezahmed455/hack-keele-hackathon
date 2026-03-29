export default function Card({ title, value }) {
  return (
    <div
      style={{
        padding: "20px",
        border: "1px solid #ddd",
        borderRadius: "8px",
        minWidth: "150px",
      }}
    >
      <h3>{title}</h3>
      <p>{value}</p>
    </div>
  );
}