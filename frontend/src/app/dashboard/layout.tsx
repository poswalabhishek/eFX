import { DashboardHeader } from "@/components/dashboard/header";

export default function DashboardLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <div className="flex flex-col h-screen">
      <DashboardHeader />
      <main className="flex-1 overflow-auto p-4 bg-muted/30">{children}</main>
    </div>
  );
}
